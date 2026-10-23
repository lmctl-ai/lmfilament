#include "dslvm.hpp"
#include "mesh.hpp"
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

struct MaterialObj { std::string name; std::vector<uint8_t> package; std::vector<std::pair<std::string, std::vector<float>>> params; };
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
  std::vector<VertexBuffer*> vbs; std::vector<IndexBuffer*> ibs; std::vector<filament::Material*> materials;
  size_t renderables = 0, lights = 0;
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
    engine->destroyCameraComponent(camera_entity); utils::EntityManager::get().destroy(camera_entity);
    if (skybox) engine->destroy(skybox);
    engine->destroy(swap); engine->destroy(view); engine->destroy(scene); engine->destroy(renderer);
    Engine::destroy(&engine);
  }
};

struct Host {
  std::vector<std::unique_ptr<MaterialObj>> materials;
  std::vector<std::unique_ptr<MeshObj>> meshes;
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
  } catch (...) { filamat::MaterialBuilder::shutdown(); throw; }
  filamat::MaterialBuilder::shutdown();
  return out;
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

  auto add_mesh = [](void* p, Args& a, float x, float y, float z) {
    auto* s = self<SceneObj>(p); auto& mesh = static_cast<MeshObj*>(a[0].obj)->mesh; auto* mat = static_cast<MaterialObj*>(a[1].obj);
    if (mesh.tangents.empty()) lmfilament::compute_tangents(mesh);
    const size_t vc = mesh.vertex_count();
    auto* mp = filament::Material::Builder().package(mat->package.data(), mat->package.size()).build(*s->engine);
    if (!mp) throw dslvm::Error("Filament rejected material " + mat->name);
    s->materials.push_back(mp);
    auto* mi = mp->getDefaultInstance();
    for (auto& [name, v] : mat->params) {
      if (!mp->hasParameter(name.c_str())) throw dslvm::Error("material " + mat->name + " has no parameter '" + name + "'");
      switch (v.size()) {
        case 1: mi->setParameter(name.c_str(), v[0]); break;
        case 3: mi->setParameter(name.c_str(), math::float3{v[0], v[1], v[2]}); break;
        case 4: mi->setParameter(name.c_str(), math::float4{v[0], v[1], v[2], v[3]}); break;
      }
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
    tcm.create(e); tcm.setTransform(tcm.getInstance(e), math::mat4f::translation(math::float3{x, y, z}));
    s->scene->addEntity(e); s->entities.push_back(e); s->renderables++;
    return Value::object("Scene", p);
  };
  checked_method(r, "Scene", "add", {"Mesh", "Material"}, "Scene", [add_mesh](void* p, Args& a) { return add_mesh(p, a, 0, 0, 0); });
  checked_method(r, "Scene", "add_at", {"Mesh", "Material", "float", "float", "float"}, "Scene", [add_mesh](void* p, Args& a) {
    for (int i = 2; i < 5; i++) if (!std::isfinite(a[i].f)) throw dslvm::Error("position must be finite");
    return add_mesh(p, a, float(a[2].f), float(a[3].f), float(a[4].f)); });
  checked_method(r, "Scene", "skybox", {"float", "float", "float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    for (int i = 0; i < 3; i++) if (!(a[i].f >= 0 && a[i].f <= 1)) throw dslvm::Error("skybox channel must be between 0 and 1");
    if (s->skybox) { s->scene->setSkybox(nullptr); s->engine->destroy(s->skybox); }
    s->skybox = Skybox::Builder().color({a[0].f, a[1].f, a[2].f, 1.0}).build(*s->engine);
    s->scene->setSkybox(s->skybox);
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
    s->scene->addEntity(e); s->entities.push_back(e); s->lights++;
    return Value::object("Scene", p);
  });
  checked_method(r, "Scene", "sun", {"float"}, "Scene", [](void* p, Args& a) {
    auto* s = self<SceneObj>(p);
    if (!(a[0].f >= 0)) throw dslvm::Error("sun intensity must not be negative");
    utils::Entity e = utils::EntityManager::get().create();
    LightManager::Builder(LightManager::Type::SUN).color({1, 1, 1}).intensity(static_cast<float>(a[0].f)).direction({0, -1, 0}).build(*s->engine, e);
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
