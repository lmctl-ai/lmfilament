#include "dslvm.hpp"
#include "mesh.hpp"
#include "preview.hpp"
#include "texture.hpp"
#include "schema_embedded.hpp"
#include <backend/DriverEnums.h>
#include <filament/Camera.h>
#include <filament/Skybox.h>
#include <filament/TransformManager.h>
#include <filament/MaterialInstance.h>
#include <math/mat4.h>
#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/LightManager.h>
#include <filament/Material.h>
#include <filament/RenderableManager.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>
#include <filament/VertexBuffer.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <filament-matp/Config.h>
#include <filament-matp/MaterialParser.h>
#include <filamat/MaterialBuilder.h>
#include <utils/EntityManager.h>
#include <utils/JobSystem.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <map>
#include <sstream>
#include <vector>

namespace {
using dslvm::Args; using dslvm::Value;
using namespace filament;

struct MatConfig : matp::Config {
  MatConfig() { mPlatform = Platform::ALL; mTargetApi = TargetApi::ALL; mOptimizationLevel = Optimization::PERFORMANCE; }
  Output* getOutput() const noexcept override { return nullptr; }
  Input* getInput() const noexcept override { return nullptr; }
  std::string toString() const noexcept override { return "lmfilament"; }
  std::string toPIISafeString() const noexcept override { return "lmfilament"; }
};

struct TextureObj { lmfilament::Texture tex; };
struct MaterialObj {
  std::string name; std::vector<uint8_t> package; std::vector<std::pair<std::string, std::vector<float>>> params;
  std::vector<std::pair<std::string, const TextureObj*>> textures;   // sampler parameter -> image (uploaded when the material is added to a scene)
  bool unlit = false;                                                 // `shadingModel : unlit` in the source: the preview draws the albedo as is
};
struct MeshObj { lmfilament::Mesh mesh; };

struct SceneObj {
  Engine* engine = nullptr;
  Renderer* renderer = nullptr;
  filament::Scene* scene = nullptr;
  View* view = nullptr;
  Camera* camera = nullptr;
  utils::Entity camera_entity;
  SwapChain* swap = nullptr;
  Skybox* skybox = nullptr;
  std::vector<utils::Entity> entities;
  std::vector<VertexBuffer*> vbs; std::vector<IndexBuffer*> ibs; std::vector<filament::Material*> materials; std::vector<filament::Texture*> textures;
  size_t renderables = 0, lights = 0;
  lmfilament::PreviewScene preview;
  std::vector<utils::Entity> node_entities;
  SceneObj() {
    engine = Engine::Builder().backend(backend::Backend::NOOP).build();
    if (!engine) throw dslvm::Error("cannot create Filament engine");
    renderer = engine->createRenderer(); scene = engine->createScene(); view = engine->createView();
    camera_entity = utils::EntityManager::get().create();
    camera = engine->createCamera(camera_entity);
    view->setScene(scene); view->setCamera(camera);
    view->setViewport({0, 0, 64, 64});
    swap = engine->createSwapChain(64, 64);
  }
  ~SceneObj() {
    if (!engine) return;
    for (auto e : entities) { scene->remove(e); engine->destroy(e); utils::EntityManager::get().destroy(e); }
    for (auto* v : vbs) engine->destroy(v);
    for (auto* i : ibs) engine->destroy(i);
    for (auto* m : materials) { engine->destroy(m); }
    for (auto* t : textures) engine->destroy(t);
    engine->destroyCameraComponent(camera_entity); utils::EntityManager::get().destroy(camera_entity);
    if (skybox) engine->destroy(skybox);
    engine->destroy(swap); engine->destroy(view); engine->destroy(scene); engine->destroy(renderer);
    Engine::destroy(&engine);
  }
};

struct PartObj { SceneObj* scene; utils::Entity entity; size_t item; };
struct NodeObj { SceneObj* scene; utils::Entity entity; size_t node; };
struct Host {
  std::vector<std::unique_ptr<PartObj>> parts;
  std::vector<std::unique_ptr<NodeObj>> nodes;
  std::vector<std::unique_ptr<MaterialObj>> materials;
  std::vector<std::unique_ptr<MeshObj>> meshes;
  std::vector<std::unique_ptr<TextureObj>> textures; size_t texture_bytes = 0;
  std::vector<std::unique_ptr<SceneObj>> scenes;
  template<class T> Value hold(std::vector<std::unique_ptr<T>>& arena, const char* type, std::unique_ptr<T> v) {
    if (arena.size() >= 4096) throw dslvm::Error(std::string(type) + " object limit exceeded (4096)");
    arena.push_back(std::move(v)); return Value::object(type, arena.back().get());
  }
};
template<class T> T* self(void* p) { return static_cast<T*>(p); }
void positive(double x, const char* name) { if (!(x > 0) || x > 1e9) throw dslvm::Error(std::string(name) + " must be positive and finite"); }

std::unique_ptr<MaterialObj> compile_material(const std::string& name, std::string source) {
  matp::MaterialParser parser; MatConfig config;
  ssize_t size = static_cast<ssize_t>(source.size());
  std::unique_ptr<char[]> owned(new char[source.size() + 1]);
  std::memcpy(owned.get(), source.c_str(), source.size() + 1);
  std::unique_ptr<const char[]> buffer(owned.release());
  parser.processTemplateSubstitutions(config, size, buffer);
  filamat::MaterialBuilder::init();
  std::unique_ptr<MaterialObj> out;
  try {
    filamat::MaterialBuilder builder;
    utils::Status status = parser.parse(builder, config, size, buffer);
    if (!status.isOk()) throw dslvm::Error("material parse error: " + std::string(status.getMessage()));
    utils::JobSystem js; js.adopt();
    filamat::Package pkg = builder.build(js);
    js.emancipate();
    if (!pkg.isValid()) throw dslvm::Error("could not compile material " + name);
    out = std::make_unique<MaterialObj>();
    out->name = name;
    out->package.assign(pkg.getData(), pkg.getData() + pkg.getSize());
    out->unlit = std::regex_search(source, std::regex("shadingModel\\s*:\\s*unlit"));
  } catch (...) { filamat::MaterialBuilder::shutdown(); throw; }
  filamat::MaterialBuilder::shutdown();
  return out;
}

math::mat4f local_matrix(const lmfilament::XForm& x) {
  const float d = float(M_PI / 180);
  return math::mat4f::translation(math::float3{float(x.t[0]), float(x.t[1]), float(x.t[2])}) *
         math::mat4f::rotation(float(x.r[2]) * d, math::float3{0, 0, 1}) * math::mat4f::rotation(float(x.r[1]) * d, math::float3{0, 1, 0}) *
         math::mat4f::rotation(float(x.r[0]) * d, math::float3{1, 0, 0}) * math::mat4f::scaling(math::float3{float(x.s[0]), float(x.s[1]), float(x.s[2])});
}
// The preview XForm is the source of truth; push it (and the parent link) into Filament's TransformManager.
void sync(SceneObj* s, utils::Entity e, const lmfilament::XForm& x) {
  auto& tcm = s->engine->getTransformManager();
  auto inst = tcm.getInstance(e);
  tcm.setTransform(inst, local_matrix(x));
  tcm.setParent(inst, x.parent >= 0 ? tcm.getInstance(s->node_entities[size_t(x.parent)]) : decltype(inst){});
}

void checked_method(dslvm::Registry& r, const char* type, const char* name, std::vector<std::string> params, std::string returns, dslvm::Method cb) {
  r.method(type, name, params, returns, [params, cb, type, name](void* object, Args& args) {
    for (size_t i = 0; i < params.size(); i++) {
      const auto& e = params[i]; const auto& v = args[i];
      bool ok = e == "float" ? v.kind == Value::FLOAT : e == "int" ? v.kind == Value::INT : e == "string" ? v.kind == Value::STR
              : e == "bool" ? v.kind == Value::BOOL : v.kind == Value::OBJ && v.type == e && v.obj;
      if (!ok) throw dslvm::Error(std::string(type) + "." + name + " argument " + std::to_string(i + 1) + " must be " + e);
    }
    try { return cb(object, args); }
    catch (const dslvm::Error&) { throw; }
    catch (const std::exception& e) { throw dslvm::Error(e.what()); }
  });
}

Value add_texture(Host& h, std::unique_ptr<TextureObj> t) {
  const size_t bytes = t->tex.rgba.size();
  if (h.texture_bytes + bytes > (size_t(1) << 29)) throw dslvm::Error("texture memory limit exceeded (512 MiB of pixels per run)");
  h.texture_bytes += bytes; return h.hold(h.textures, "Texture", std::move(t));
}
void register_host(dslvm::Registry& r, Host& h) {
  r.context("fil", "Filament");
  checked_method(r, "Filament", "version", {}, "string", [](void*, Args&) { return Value::string(LMFILAMENT_FILAMENT_VERSION); });
  checked_method(r, "Filament", "material", {"string", "string"}, "Material", [&h](void*, Args& a) {
    if (a[0].s.empty()) throw dslvm::Error("material name must not be empty");
    return h.hold(h.materials, "Material", compile_material(a[0].s, a[1].s));
  });
  checked_method(r, "Filament", "material_file", {"string"}, "Material", [&h](void*, Args& a) {
    std::ifstream in(a[0].s, std::ios::binary);
    if (!in) throw dslvm::Error("cannot open " + a[0].s);
    std::stringstream ss; ss << in.rdbuf();
    return h.hold(h.materials, "Material", compile_material(a[0].s, ss.str()));
  });
  checked_method(r, "Filament", "box", {"float", "float", "float"}, "Mesh", [&h](void*, Args& a) {
    for (int i = 0; i < 3; i++) positive(a[i].f, "box dimension");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_box(a[0].f, a[1].f, a[2].f);
    return h.hold(h.meshes, "Mesh", std::move(m));
  });
  checked_method(r, "Filament", "sphere", {"float", "int"}, "Mesh", [&h](void*, Args& a) {
    positive(a[0].f, "sphere radius");
    if (a[1].i < 3 || a[1].i > 512) throw dslvm::Error("sphere segments must be between 3 and 512");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_sphere(a[0].f, static_cast<int>(a[1].i));
    return h.hold(h.meshes, "Mesh", std::move(m));
  });
  checked_method(r, "Filament", "plane", {"float", "float"}, "Mesh", [&h](void*, Args& a) {
    positive(a[0].f, "plane width"); positive(a[1].f, "plane depth");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_plane(a[0].f, a[1].f);
    return h.hold(h.meshes, "Mesh", std::move(m));
  });
  checked_method(r, "Filament", "scene", {}, "Scene", [&h](void*, Args&) { return h.hold(h.scenes, "Scene", std::make_unique<SceneObj>()); });

  checked_method(r, "Material", "name", {}, "string", [](void* p, Args&) { return Value::string(self<MaterialObj>(p)->name); });
  checked_method(r, "Material", "package_size", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<int64_t>(self<MaterialObj>(p)->package.size())); });
  checked_method(r, "Material", "save", {"string"}, "void", [](void* p, Args& a) {
    if (a[0].s.empty()) throw dslvm::Error("output path must not be empty");
    std::ofstream out(a[0].s, std::ios::binary);
    auto& pkg = self<MaterialObj>(p)->package;
    out.write(reinterpret_cast<const char*>(pkg.data()), static_cast<std::streamsize>(pkg.size()));
    if (!out) throw dslvm::Error("write failed: " + a[0].s);
    return Value::none();
  });

  auto set_param = [](const char* name, size_t n) {
    return [name, n](void* p, Args& a) {
      if (a[0].s.empty()) throw dslvm::Error(std::string(name) + ": parameter name must not be empty");
      std::vector<float> v;
      for (size_t i = 1; i <= n; i++) { if (!std::isfinite(a[i].f)) throw dslvm::Error("material parameter must be finite"); v.push_back(static_cast<float>(a[i].f)); }
      auto* m = self<MaterialObj>(p);
      for (auto& kv : m->params) if (kv.first == a[0].s) { kv.second = v; return Value::object("Material", p); }
      m->params.emplace_back(a[0].s, v);
      return Value::object("Material", p);
    };
  };
  checked_method(r, "Material", "set_float", {"string", "float"}, "Material", set_param("set_float", 1));
  checked_method(r, "Material", "set_float3", {"string", "float", "float", "float"}, "Material", set_param("set_float3", 3));
  checked_method(r, "Material", "set_float4", {"string", "float", "float", "float", "float"}, "Material", set_param("set_float4", 4));
  checked_method(r, "Filament", "texture", {"int", "int"}, "Texture", [&h](void*, Args& a) {
    if (a[0].i < 1 || a[1].i < 1 || a[0].i > 4096 || a[1].i > 4096) throw dslvm::Error("texture size must be between 1 and 4096 per side");
    auto t = std::make_unique<TextureObj>(); t->tex.width = int(a[0].i); t->tex.height = int(a[1].i); t->tex.rgba.assign(size_t(t->tex.width) * t->tex.height * 4, 255);
    return add_texture(h, std::move(t)); });
  checked_method(r, "Filament", "texture_file", {"string"}, "Texture", [&h](void*, Args& a) {
    auto t = std::make_unique<TextureObj>(); t->tex = lmfilament::load_texture(a[0].s); return add_texture(h, std::move(t)); });
  checked_method(r, "Filament", "texture_checker", {"int", "int", "float", "float", "float", "float", "float", "float"}, "Texture", [&h](void*, Args& a) {
    if (a[0].i < 1 || a[0].i > 4096) throw dslvm::Error("texture size must be between 1 and 4096");
    if (a[1].i < 1 || a[1].i > a[0].i) throw dslvm::Error("checker cell must be between 1 and the texture size");
    for (int i = 2; i < 8; i++) if (!(a[i].f >= 0 && a[i].f <= 1)) throw dslvm::Error("checker colour must be between 0 and 1");
    auto t = std::make_unique<TextureObj>();
    t->tex = lmfilament::Texture::checker(int(a[0].i), int(a[1].i), {float(a[2].f), float(a[3].f), float(a[4].f)}, {float(a[5].f), float(a[6].f), float(a[7].f)});
    return add_texture(h, std::move(t)); });
  checked_method(r, "Texture", "pixel", {"int", "int", "float", "float", "float"}, "Texture", [](void* p, Args& a) {
    auto& t = self<TextureObj>(p)->tex;
    if (a[0].i < 0 || a[1].i < 0 || a[0].i >= t.width || a[1].i >= t.height) throw dslvm::Error("pixel coordinates are outside the texture");
    for (int i = 2; i < 5; i++) if (!(a[i].f >= 0 && a[i].f <= 1)) throw dslvm::Error("pixel colour must be between 0 and 1");
    const size_t o = (size_t(a[1].i) * t.width + size_t(a[0].i)) * 4;
    for (int c = 0; c < 3; c++) t.rgba[o + c] = static_cast<uint8_t>(std::lround(a[2 + c].f * 255.0));
    t.rgba[o + 3] = 255; t.dirty = true; return Value::object("Texture", p); });
  checked_method(r, "Texture", "filter", {"string"}, "Texture", [](void* p, Args& a) {
    if (a[0].s == "linear") self<TextureObj>(p)->tex.linear = true; else if (a[0].s == "nearest") self<TextureObj>(p)->tex.linear = false;
    else throw dslvm::Error("texture filter must be \"linear\" or \"nearest\"");
    return Value::object("Texture", p); });
  checked_method(r, "Texture", "wrap", {"string"}, "Texture", [](void* p, Args& a) {
    auto& t = self<TextureObj>(p)->tex;
    if (a[0].s == "repeat") t.wrap = lmfilament::Texture::Wrap::Repeat; else if (a[0].s == "clamp") t.wrap = lmfilament::Texture::Wrap::Clamp; else if (a[0].s == "mirror") t.wrap = lmfilament::Texture::Wrap::Mirror;
    else throw dslvm::Error("texture wrap must be \"repeat\", \"clamp\" or \"mirror\"");
    return Value::object("Texture", p); });
  checked_method(r, "Texture", "repeat", {"float", "float"}, "Texture", [](void* p, Args& a) {
    positive(a[0].f, "repeat u"); positive(a[1].f, "repeat v"); self<TextureObj>(p)->tex.scale_u = float(a[0].f); self<TextureObj>(p)->tex.scale_v = float(a[1].f); return Value::object("Texture", p); });
  checked_method(r, "Texture", "width", {}, "int", [](void* p, Args&) { return Value::integer(self<TextureObj>(p)->tex.width); });
  checked_method(r, "Texture", "height", {}, "int", [](void* p, Args&) { return Value::integer(self<TextureObj>(p)->tex.height); });
  checked_method(r, "Texture", "save_png", {"string"}, "void", [](void* p, Args& a) {
    const auto& t = self<TextureObj>(p)->tex; std::vector<unsigned char> rgb(size_t(t.width) * t.height * 3);
    for (size_t i = 0; i < size_t(t.width) * t.height; i++) for (int c = 0; c < 3; c++) rgb[i * 3 + c] = t.rgba[i * 4 + c];
    lmfilament::write_png_rgb(a[0].s, t.width, t.height, rgb); return Value::none(); });
  checked_method(r, "Material", "set_texture", {"string", "Texture"}, "Material", [](void* p, Args& a) {
    if (a[0].s.empty()) throw dslvm::Error("set_texture: parameter name must not be empty");
    auto* m = self<MaterialObj>(p); const auto* t = static_cast<const TextureObj*>(a[1].obj);
    for (auto& kv : m->textures) if (kv.first == a[0].s) { kv.second = t; return Value::object("Material", p); }
    m->textures.emplace_back(a[0].s, t); return Value::object("Material", p); });
  checked_method(r, "Filament", "cylinder", {"float", "float", "int"}, "Mesh", [&h](void*, Args& a) { positive(a[0].f, "cylinder radius"); positive(a[1].f, "cylinder height");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_cylinder(float(a[0].f), float(a[1].f), int(a[2].i)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "cone", {"float", "float", "int"}, "Mesh", [&h](void*, Args& a) { positive(a[0].f, "cone radius"); positive(a[1].f, "cone height");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_cone(float(a[0].f), float(a[1].f), int(a[2].i)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "capsule", {"float", "float", "int"}, "Mesh", [&h](void*, Args& a) { positive(a[0].f, "capsule radius"); positive(a[1].f, "capsule length");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_capsule(float(a[0].f), float(a[1].f), int(a[2].i)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "icosphere", {"float", "int"}, "Mesh", [&h](void*, Args& a) { positive(a[0].f, "icosphere radius");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_icosphere(float(a[0].f), int(a[1].i)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "tube_between", {"float", "float", "float", "float", "float", "float", "float"}, "Mesh", [&h](void*, Args& a) {
    for (int i = 0; i < 6; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error("tube_between argument must be finite");
    positive(a[6].f, "tube radius");
    const float p0[3] = {float(a[0].f), float(a[1].f), float(a[2].f)}, p1[3] = {float(a[3].f), float(a[4].f), float(a[5].f)};
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_tube(p0, p1, float(a[6].f)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "ring", {"float", "float", "int"}, "Mesh", [&h](void*, Args& a) {
    positive(a[0].f, "ring inner radius"); positive(a[1].f, "ring outer radius");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_ring(float(a[0].f), float(a[1].f), int(a[2].i)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "text", {"string", "float"}, "Mesh", [&h](void*, Args& a) { positive(a[1].f, "text height");
    if (a[0].s.empty() || a[0].s.size() > 256) throw dslvm::Error("text must be 1 to 256 characters");
    auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_text(a[0].s, float(a[1].f)); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "triangle", {}, "Mesh", [&h](void*, Args&) { auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::make_triangle(); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Filament", "mesh_obj", {"string"}, "Mesh", [&h](void*, Args& a) { auto m = std::make_unique<MeshObj>(); m->mesh = lmfilament::load_obj(a[0].s); return h.hold(h.meshes, "Mesh", std::move(m)); });
  checked_method(r, "Mesh", "has_colors", {}, "bool", [](void* p, Args&) { return Value::boolean(!self<MeshObj>(p)->mesh.colors.empty()); });
  checked_method(r, "Mesh", "vertex_count", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<int64_t>(self<MeshObj>(p)->mesh.vertex_count())); });
  checked_method(r, "Mesh", "triangle_count", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<int64_t>(self<MeshObj>(p)->mesh.triangle_count())); });
  checked_method(r, "Mesh", "has_tangents", {}, "bool", [](void* p, Args&) { return Value::boolean(!self<MeshObj>(p)->mesh.tangents.empty()); });
  checked_method(r, "Mesh", "compute_tangents", {}, "Mesh", [](void* p, Args&) { lmfilament::compute_tangents(self<MeshObj>(p)->mesh); return Value::object("Mesh", p); });
  checked_method(r, "Mesh", "save_obj", {"string"}, "void", [](void* p, Args& a) {
    if (a[0].s.empty()) throw dslvm::Error("output path must not be empty");
    lmfilament::write_obj(a[0].s, self<MeshObj>(p)->mesh); return Value::none();
  });

  auto add_part = [&h](void* p, Args& a) -> PartObj* {
    auto* s = self<SceneObj>(p); auto& mesh = static_cast<MeshObj*>(a[0].obj)->mesh; auto* mat = static_cast<MaterialObj*>(a[1].obj);
    if (mesh.tangents.empty()) lmfilament::compute_tangents(mesh);
    const size_t vc = mesh.vertex_count();
    auto* mp = filament::Material::Builder().package(mat->package.data(), mat->package.size()).build(*s->engine);
    if (!mp) throw dslvm::Error("Filament rejected material " + mat->name);
    s->materials.push_back(mp);
    auto* mi = mp->getDefaultInstance();
    std::vector<filament::Material::ParameterInfo> infos(mp->getParameterCount());
    mp->getParameters(infos.data(), infos.size());
    auto info_of = [&](const std::string& name) -> const filament::Material::ParameterInfo* {
      for (const auto& i : infos) if (name == i.name) return &i;
      return nullptr;
    };
    for (auto& [name, v] : mat->params) {
      const auto* info = info_of(name);
      if (!info) throw dslvm::Error("material " + mat->name + " has no parameter '" + name + "'");
      if (info->isSampler) throw dslvm::Error("material " + mat->name + " parameter '" + name + "' is a sampler: use set_texture");
      switch (v.size()) {
        case 1: mi->setParameter(name.c_str(), v[0]); break;
        case 3: mi->setParameter(name.c_str(), math::float3{v[0], v[1], v[2]}); break;
        case 4: mi->setParameter(name.c_str(), math::float4{v[0], v[1], v[2], v[3]}); break;
      }
    }
    const lmfilament::Texture* albedo = nullptr; int albedo_rank = 99;
    for (auto& [name, tobj] : mat->textures) {
      const auto* info = info_of(name);
      if (!info) throw dslvm::Error("material " + mat->name + " has no parameter '" + name + "'");
      if (!info->isSampler) throw dslvm::Error("material " + mat->name + " parameter '" + name + "' is not a sampler");
      if (info->samplerType != filament::Material::SamplerType::SAMPLER_2D) throw dslvm::Error("material " + mat->name + " parameter '" + name + "': only sampler2d parameters take a texture");
      const lmfilament::Texture& t = tobj->tex;
      static const char* const color_names[] = {"albedo", "baseColor", "color", "emissive"};
      int rank = 99; for (int k = 0; k < 4; k++) if (name == color_names[k]) rank = k;
      auto* ft = filament::Texture::Builder().width(uint32_t(t.width)).height(uint32_t(t.height)).levels(uint8_t(t.mip_count()))
          .format(rank < 99 ? filament::Texture::InternalFormat::SRGB8_A8 : filament::Texture::InternalFormat::RGBA8)
          .sampler(filament::Texture::Sampler::SAMPLER_2D).usage(filament::Texture::Usage::DEFAULT | filament::Texture::Usage::GEN_MIPMAPPABLE).build(*s->engine);
      if (!ft) throw dslvm::Error("Filament rejected the texture for '" + name + "'");
      auto* copy = new std::vector<uint8_t>(t.rgba);
      ft->setImage(*s->engine, 0, filament::Texture::PixelBufferDescriptor(copy->data(), copy->size(), filament::Texture::Format::RGBA, filament::Texture::Type::UBYTE,
          [](void*, size_t, void* u) { delete static_cast<std::vector<uint8_t>*>(u); }, copy));
      ft->generateMipmaps(*s->engine);
      s->textures.push_back(ft);
      using MinF = filament::TextureSampler::MinFilter; using MagF = filament::TextureSampler::MagFilter; using Wm = filament::TextureSampler::WrapMode;
      const Wm wm = t.wrap == lmfilament::Texture::Wrap::Clamp ? Wm::CLAMP_TO_EDGE : t.wrap == lmfilament::Texture::Wrap::Mirror ? Wm::MIRRORED_REPEAT : Wm::REPEAT;
      mi->setParameter(name.c_str(), ft, filament::TextureSampler(t.linear ? MinF::LINEAR_MIPMAP_LINEAR : MinF::NEAREST, t.linear ? MagF::LINEAR : MagF::NEAREST, wm));
      if (rank < albedo_rank || (!albedo && rank == 99)) { albedo = &t; albedo_rank = rank; }
    }
    const bool colors = !mesh.colors.empty();
    VertexBuffer::Builder vbb;
    vbb.vertexCount(static_cast<uint32_t>(vc)).bufferCount(colors ? 4 : 3)
        .attribute(VertexAttribute::POSITION, 0, VertexBuffer::AttributeType::FLOAT3, 0, 12)
        .attribute(VertexAttribute::TANGENTS, 1, VertexBuffer::AttributeType::SHORT4, 0, 8).normalized(VertexAttribute::TANGENTS)
        .attribute(VertexAttribute::UV0, 2, VertexBuffer::AttributeType::FLOAT2, 0, 8);
    if (colors) vbb.attribute(VertexAttribute::COLOR, 3, VertexBuffer::AttributeType::UBYTE4, 0, 4).normalized(VertexAttribute::COLOR);
    auto* vb = vbb.build(*s->engine);
    // Filament consumes the buffers asynchronously: hand it heap copies it frees via callback.
    auto give = [&](uint8_t slot, const auto& vec) {
      using V = std::decay_t<decltype(vec)>;
      auto* d = new V(vec);
      vb->setBufferAt(*s->engine, slot, VertexBuffer::BufferDescriptor(d->data(), d->size() * sizeof(typename V::value_type),
          [](void*, size_t, void* u) { delete static_cast<V*>(u); }, d));
    };
    give(0, mesh.positions); give(1, mesh.tangents); give(2, mesh.uvs);
    if (colors) give(3, mesh.colors);
    auto* idx = new std::vector<uint32_t>(mesh.indices);
    auto* ib = IndexBuffer::Builder().indexCount(static_cast<uint32_t>(idx->size())).bufferType(IndexBuffer::IndexType::UINT).build(*s->engine);
    ib->setBuffer(*s->engine, IndexBuffer::BufferDescriptor(idx->data(), idx->size() * 4, [](void*, size_t, void* u) { delete static_cast<std::vector<uint32_t>*>(u); }, idx));
    s->vbs.push_back(vb); s->ibs.push_back(ib);
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t i = 0; i < vc; i++) for (int k = 0; k < 3; k++) { lo[k] = std::min(lo[k], mesh.positions[3 * i + k]); hi[k] = std::max(hi[k], mesh.positions[3 * i + k]); }
    utils::Entity e = utils::EntityManager::get().create();
    RenderableManager::Builder(1)
        .boundingBox({{(lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2}, {(hi[0] - lo[0]) / 2, (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2}})
        .material(0, mi)
        .geometry(0, RenderableManager::PrimitiveType::TRIANGLES, vb, ib)
        .build(*s->engine, e);
    auto& tcm = s->engine->getTransformManager();
    tcm.create(e);
    {
      lmfilament::PreviewItem item; item.mesh = mesh; item.texture = albedo; item.unlit = mat->unlit;
      for (auto& [name, v] : mat->params) if ((name == "baseColor" || name == "color") && v.size() >= 3) item.base_color = {v[0], v[1], v[2]};
      s->preview.items.push_back(std::move(item));
    }
    s->scene->addEntity(e); s->entities.push_back(e); s->renderables++;
    h.parts.push_back(std::make_unique<PartObj>(PartObj{s, e, s->preview.items.size() - 1}));
    return h.parts.back().get();
  };
  checked_method(r, "Scene", "add", {"Mesh", "Material"}, "Scene", [add_part](void* p, Args& a) { add_part(p, a); return Value::object("Scene", p); });
  checked_method(r, "Scene", "add_at", {"Mesh", "Material", "float", "float", "float"}, "Scene", [add_part](void* p, Args& a) {
    for (int i = 2; i < 5; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error("position must be finite");
    PartObj* part = add_part(p, a);
    auto& xf = part->scene->preview.items[part->item].xf; xf.t = {a[2].f, a[3].f, a[4].f};
    sync(part->scene, part->entity, xf);
    return Value::object("Scene", p); });
  checked_method(r, "Scene", "add_part", {"Mesh", "Material"}, "Part", [&h, add_part](void* p, Args& a) { return Value::object("Part", add_part(p, a)); });
  checked_method(r, "Scene", "node", {}, "Node", [&h](void* p, Args&) {
    auto* sc = self<SceneObj>(p);
    utils::Entity e = utils::EntityManager::get().create();
    sc->engine->getTransformManager().create(e);
    sc->entities.push_back(e); sc->node_entities.push_back(e); sc->preview.nodes.emplace_back();
    h.nodes.push_back(std::make_unique<NodeObj>(NodeObj{sc, e, sc->preview.nodes.size() - 1}));
    return Value::object("Node", h.nodes.back().get()); });
  auto finite3 = [](Args& a, const char* what) { for (int i = 0; i < 3; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error(std::string(what) + " must be finite"); };
  auto node_xf = [](void* p) -> lmfilament::XForm& { auto* n = self<NodeObj>(p); return n->scene->preview.nodes[n->node]; };
  auto part_xf = [](void* p) -> lmfilament::XForm& { auto* q = self<PartObj>(p); return q->scene->preview.items[q->item].xf; };
  auto node_sync = [node_xf](void* p) { auto* n = self<NodeObj>(p); sync(n->scene, n->entity, node_xf(p)); };
  auto part_sync = [part_xf](void* p) { auto* q = self<PartObj>(p); sync(q->scene, q->entity, part_xf(p)); };
  checked_method(r, "Node", "translate", {"float", "float", "float"}, "Node", [=](void* p, Args& a) { finite3(a, "translate"); node_xf(p).t = {a[0].f, a[1].f, a[2].f}; node_sync(p); return Value::object("Node", p); });
  checked_method(r, "Node", "rotate", {"float", "float", "float"}, "Node", [=](void* p, Args& a) { finite3(a, "rotate"); node_xf(p).r = {a[0].f, a[1].f, a[2].f}; node_sync(p); return Value::object("Node", p); });
  checked_method(r, "Node", "scale", {"float", "float", "float"}, "Node", [=](void* p, Args& a) { finite3(a, "scale"); for (int i = 0; i < 3; i++) positive(a[i].f, "scale"); node_xf(p).s = {a[0].f, a[1].f, a[2].f}; node_sync(p); return Value::object("Node", p); });
  checked_method(r, "Node", "parent", {"Node"}, "Node", [=](void* p, Args& a) {
    auto* n = self<NodeObj>(p); auto* np = static_cast<NodeObj*>(a[0].obj);
    if (np->scene != n->scene) throw dslvm::Error("node parent must belong to the same scene");
    for (int i = int(np->node); i >= 0; i = n->scene->preview.nodes[size_t(i)].parent) if (size_t(i) == n->node) throw dslvm::Error("node parent would create a cycle");
    node_xf(p).parent = int(np->node); node_sync(p); return Value::object("Node", p); });
  checked_method(r, "Part", "translate", {"float", "float", "float"}, "Part", [=](void* p, Args& a) { finite3(a, "translate"); part_xf(p).t = {a[0].f, a[1].f, a[2].f}; part_sync(p); return Value::object("Part", p); });
  checked_method(r, "Part", "rotate", {"float", "float", "float"}, "Part", [=](void* p, Args& a) { finite3(a, "rotate"); part_xf(p).r = {a[0].f, a[1].f, a[2].f}; part_sync(p); return Value::object("Part", p); });
  checked_method(r, "Part", "scale", {"float", "float", "float"}, "Part", [=](void* p, Args& a) { finite3(a, "scale"); for (int i = 0; i < 3; i++) positive(a[i].f, "scale"); part_xf(p).s = {a[0].f, a[1].f, a[2].f}; part_sync(p); return Value::object("Part", p); });
  checked_method(r, "Part", "visible", {"bool"}, "Part", [](void* p, Args& a) {
    auto* q = self<PartObj>(p); auto& item = q->scene->preview.items[q->item];
    if (item.visible != a[0].b) {
      if (a[0].b) q->scene->scene->addEntity(q->entity); else q->scene->scene->remove(q->entity);
      item.visible = a[0].b;
    }
    return Value::object("Part", p); });
  checked_method(r, "Part", "parent", {"Node"}, "Part", [=](void* p, Args& a) {
    auto* q = self<PartObj>(p); auto* np = static_cast<NodeObj*>(a[0].obj);
    if (np->scene != q->scene) throw dslvm::Error("node parent must belong to the same scene");
    part_xf(p).parent = int(np->node); part_sync(p); return Value::object("Part", p); });
  checked_method(r, "Scene", "skybox", {"float", "float", "float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    for (int i = 0; i < 3; i++) if (!(a[i].f >= 0 && a[i].f <= 1)) throw dslvm::Error("skybox channel must be between 0 and 1");
    if (s->skybox) { s->scene->setSkybox(nullptr); s->engine->destroy(s->skybox); }
    s->skybox = Skybox::Builder().color({a[0].f, a[1].f, a[2].f, 1.0}).build(*s->engine);
    s->scene->setSkybox(s->skybox);
    s->preview.sky = {float(a[0].f), float(a[1].f), float(a[2].f)};
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "camera", {"float", "float", "float", "float", "float", "float", "float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    for (int i = 0; i < 7; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error("camera argument must be finite");
    if (a[0].f <= 0 || a[0].f >= 179) throw dslvm::Error("camera field of view must be between 0 and 179 degrees");
    const math::double3 eye{a[1].f, a[2].f, a[3].f}, target{a[4].f, a[5].f, a[6].f};
    if (length(eye - target) == 0) throw dslvm::Error("camera eye and target must differ");
    s->camera->setProjection(a[0].f, 1.0, 0.1, 1000.0, Camera::Fov::VERTICAL);
    s->camera->lookAt(eye, target, math::double3{0, 1, 0});
    s->preview.fov_degrees = float(a[0].f); s->preview.eye = {eye.x, eye.y, eye.z}; s->preview.target = {target.x, target.y, target.z};
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "directional_light", {"float", "float", "float", "float", "float", "float", "float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    for (int i = 0; i < 3; i++) if (!(a[i].f >= 0 && a[i].f <= 1)) throw dslvm::Error("light color channel must be between 0 and 1");
    if (!(a[3].f >= 0)) throw dslvm::Error("light intensity must not be negative");
    const math::float3 d{float(a[4].f), float(a[5].f), float(a[6].f)};
    if (length(d) == 0) throw dslvm::Error("light direction must be nonzero");
    utils::Entity e = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::DIRECTIONAL).color({float(a[0].f), float(a[1].f), float(a[2].f)}).intensity(float(a[3].f)).direction(normalize(d)).build(*s->engine, e);
    s->preview.lights.push_back({true, {float(a[0].f), float(a[1].f), float(a[2].f)}, {d.x, d.y, d.z}, float(a[3].f), 0});
    s->scene->addEntity(e); s->entities.push_back(e); s->lights++;
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "point_light", {"float", "float", "float", "float", "float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    for (int i = 0; i < 3; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error("light position must be finite");
    if (!(a[3].f >= 0)) throw dslvm::Error("light intensity must not be negative");
    if (!(a[4].f > 0)) throw dslvm::Error("light falloff radius must be positive");
    utils::Entity e = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::POINT).position({float(a[0].f), float(a[1].f), float(a[2].f)}).intensity(float(a[3].f)).falloff(float(a[4].f)).build(*s->engine, e);
    s->preview.lights.push_back({false, {1, 1, 1}, {float(a[0].f), float(a[1].f), float(a[2].f)}, float(a[3].f), float(a[4].f)});
    s->scene->addEntity(e); s->entities.push_back(e); s->lights++;
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "shadows", {"bool"}, "Scene", [](void* p, Args& a) { self<SceneObj>(p)->preview.shadows = a[0].b; return Value::object("Scene", p); });
  checked_method(r, "Scene", "sun", {"float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    if (!(a[0].f >= 0)) throw dslvm::Error("sun intensity must not be negative");
    utils::Entity e = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN).color({1, 1, 1}).intensity(static_cast<float>(a[0].f)).direction({0, -1, 0}).build(*s->engine, e);
    s->preview.lights.push_back({true, {1, 1, 1}, {0, -1, 0}, float(a[0].f), 0});
    s->scene->addEntity(e); s->entities.push_back(e); s->lights++;
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "render_frames", {"int"}, "int", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    if (a[0].i < 0 || a[0].i > 100000) throw dslvm::Error("frame count must be between 0 and 100000");
    int64_t drawn = 0;
    for (int64_t i = 0; i < a[0].i; i++) {
      if (s->renderer->beginFrame(s->swap)) { s->renderer->render(s->view); s->renderer->endFrame(); drawn++; }
    }
    s->engine->flushAndWait();
    return Value::integer(drawn);
  });
  checked_method(r, "Scene", "render_png", {"string", "int", "int"}, "void", [](void* p, Args& a) {
    if (a[1].i < 1 || a[2].i < 1 || a[1].i > 4096 || a[2].i > 4096) throw dslvm::Error("image dimensions must be between 1 and 4096");
    lmfilament::render_png(self<SceneObj>(p)->preview, static_cast<int>(a[1].i), static_cast<int>(a[2].i), a[0].s);
    return Value::none();
  });
  checked_method(r, "Scene", "renderable_count", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<int64_t>(self<SceneObj>(p)->renderables)); });
  checked_method(r, "Scene", "light_count", {}, "int", [](void* p, Args&) { return Value::integer(static_cast<int64_t>(self<SceneObj>(p)->lights)); });
}

void print_schema_summary(const dslvm::Registry& reg) {
  std::string current; std::vector<std::string> order; std::map<std::string, std::vector<std::string>> members;
  for (const auto& sig : reg.signatures()) {
    if (sig.rfind("context ", 0) == 0 || sig.rfind("fn ", 0) == 0) std::cout << sig << "\n";
    else if (sig.rfind("method ", 0) == 0) { std::string body = sig.substr(7), type = body.substr(0, body.find('.')); if (type != current) { current = type; order.push_back(type); } members[type].push_back(body); }
  }
  for (const auto& t : order) { std::cout << t << ":\n"; for (const auto& m : members[t]) std::cout << "  " << m << "\n"; }
}
}  // namespace

int main(int argc, char** argv) {
  Host host;
  dslvm::Registry reg;
  register_host(reg, host);
  const std::string usage = "usage: lmfilament --version | --dsl-schema [--json] | --dsl-signatures | --dsl FILE [FILE ...]\n";
  if (argc == 2 && std::string(argv[1]) == "--version") { std::cout << "lmfilament " LMFILAMENT_VERSION " (filament " LMFILAMENT_FILAMENT_VERSION ")\n"; return std::cout ? 0 : 4; }
  if (argc >= 2 && std::string(argv[1]) == "--dsl-schema") {
    if (argc == 2) { print_schema_summary(reg); return std::cout ? 0 : 4; }
    if (argc == 3 && std::string(argv[2]) == "--json") { std::cout.write(reinterpret_cast<const char*>(lmfilament::embedded_schema), static_cast<std::streamsize>(lmfilament::embedded_schema_size)); return std::cout ? 0 : 4; }
    std::cerr << usage; return 2;
  }
  if (argc == 2 && std::string(argv[1]) == "--dsl-signatures") { for (const auto& s : reg.signatures()) std::cout << s << "\n"; return 0; }
  if (argc < 3 || std::string(argv[1]) != "--dsl") { std::cerr << usage; return 2; }
  for (int i = 2; i < argc; i++) if (std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) { std::cerr << usage; return 2; }
  const std::string schema(reinterpret_cast<const char*>(lmfilament::embedded_schema), lmfilament::embedded_schema_size);
  dslvm::Context context; context.set("fil", &host);
  for (int i = 2; i < argc; i++) {
    dslvm::Program program;
    try { program = dslvm::Program::load(dslvm::read_file(argv[i]), schema, reg); }
    catch (const dslvm::Error& e) { return dslvm::report_error("load", e); }
    try { program.run(context, reg); }
    catch (const dslvm::Error& e) { return dslvm::report_error("run", e); }
    dslvm::report_ok();
  }
  return 0;
}
