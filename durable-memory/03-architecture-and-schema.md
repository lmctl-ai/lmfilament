# Architecture and schema

## Source layout

| file | role |
| --- | --- |
| `schema/lmfilament.dsl.json` | the contract: types, methods, context `fil: Filament` |
| `cpp/dslvm.hpp` | vendored SDK header (do not edit; `sdk.lock`) |
| `cpp/main.cpp` | `Host` arenas, Filament glue, method registration, CLI |
| `cpp/mesh.{hpp,cpp}` | procedural meshes, OBJ read/write, tangent frames |
| `cpp/preview.{hpp,cpp}`, `cpp/deflate.hpp` | CPU preview rasterizer + PNG writer (deflate from lmcad, MIT) |
| `cpp/texture.{hpp,cpp}`, `cpp/stb_image.h` | `Texture` images (8-bit RGBA), stb_image decode, mip chain and wrap/filter sampling for the preview |
| `scripts/` | `build-filament.sh`, `build.sh`, `sync-sdk.sh` |
| `test.sh` | pinned-hash, static-link, signature, example and negative checks |

CMake turns the schema file into `generated/schema_embedded.hpp` at configure time; changing the schema
re-runs configure.

## Object model

`Host` owns arenas of `unique_ptr<T>` for `MaterialObj`, `MeshObj`, `SceneObj` (max 4096 each) and hands
out raw pointers wrapped in `Value::object(type, ptr)`. Objects live until process exit; DSL values never
own anything. `checked_method` wraps every registration: it type-checks arguments (the VM does not) and
converts `std::exception` into `dslvm::Error` so failures surface as the JSON error, not a crash.

- `MaterialObj`: the compiled package bytes + named float parameters set from the DSL.
- `MeshObj`: CPU arrays (positions, normals, uvs, optional packed colors, tangents).
- `SceneObj`: one noop `Engine` per scene, plus Renderer/Scene/View/Camera/SwapChain and the buffers,
  materials, entities it created; its destructor tears them down in dependency order.

## Material path

`Filament.material(name, source)` / `material_file(path)` run upstream's `matp::MaterialParser`
(`filament-matp`) with a minimal `matp::Config` (Platform ALL, TargetApi ALL, Optimization PERFORMANCE),
then `filamat::MaterialBuilder::build`. `MaterialBuilder::init()/shutdown()` bracket each compile (glslang
global state). `Material.set_float/float3/float4` only record values; `Scene.add` creates a
`filament::Material` from the package, verifies each name with `hasParameter`, and applies them to the
default instance. So parameters are validated at `add` time, with the DSL line of the `add` call.

## Scene.add

Computes tangents if missing, then creates a `VertexBuffer` (POSITION float3, TANGENTS short4 normalized,
UV0 float2, optional COLOR ubyte4 normalized, one buffer per attribute), a 32-bit `IndexBuffer`, a
renderable with a bounding box from the positions, and a `TransformManager` translation for `add_at`.
Buffer data is copied to the heap and freed in Filament's buffer callbacks (Filament reads them
asynchronously). Every mesh therefore binds tangents and uv0, so any lit material works; a material that
requires another attribute (e.g. `uv1`, `color` on a mesh without colors) is not yet supported.

`SceneObj::preview` (a `PreviewScene`) mirrors what `add`, `skybox`, `camera` and the light methods give Filament, so
`render_png` needs no Filament readback (the noop backend has nothing to read).

## Schema surface (generated from `build/host/lmfilament --dsl-signatures`)

```
Filament:
  box(float, float, float) -> Mesh
  capsule(float, float, int) -> Mesh
  cone(float, float, int) -> Mesh
  cylinder(float, float, int) -> Mesh
  icosphere(float, int) -> Mesh
  material(string, string) -> Material
  material_file(string) -> Material
  mesh_obj(string) -> Mesh
  plane(float, float) -> Mesh
  ring(float, float, int) -> Mesh
  scene() -> Scene
  sphere(float, int) -> Mesh
  text(string, float) -> Mesh
  texture(int, int) -> Texture
  texture_checker(int, int, float, float, float, float, float, float) -> Texture
  texture_file(string) -> Texture
  triangle() -> Mesh
  tube_between(float, float, float, float, float, float, float) -> Mesh
  version() -> string
Material:
  name() -> string
  package_size() -> int
  save(string) -> void
  set_float(string, float) -> Material
  set_float3(string, float, float, float) -> Material
  set_float4(string, float, float, float, float) -> Material
  set_texture(string, Texture) -> Material
Mesh:
  compute_tangents() -> Mesh
  has_colors() -> bool
  has_tangents() -> bool
  save_obj(string) -> void
  triangle_count() -> int
  vertex_count() -> int
Node:
  parent(Node) -> Node
  rotate(float, float, float) -> Node
  scale(float, float, float) -> Node
  translate(float, float, float) -> Node
Part:
  parent(Node) -> Part
  rotate(float, float, float) -> Part
  scale(float, float, float) -> Part
  translate(float, float, float) -> Part
  visible(bool) -> Part
Scene:
  add(Mesh, Material) -> Scene
  add_at(Mesh, Material, float, float, float) -> Scene
  add_part(Mesh, Material) -> Part
  camera(float, float, float, float, float, float, float) -> Scene
  directional_light(float, float, float, float, float, float, float) -> Scene
  light_count() -> int
  node() -> Node
  point_light(float, float, float, float, float) -> Scene
  render_frames(int) -> int
  render_png(string, int, int) -> void
  renderable_count() -> int
  shadows(bool) -> Scene
  skybox(float, float, float) -> Scene
  sun(float) -> Scene
Texture:
  filter(string) -> Texture
  height() -> int
  pixel(int, int, float, float, float) -> Texture
  repeat(float, float) -> Texture
  save_png(string) -> void
  width() -> int
  wrap(string) -> Texture
```

## Textures

`TextureObj` (host arena, max 512 MiB of pixels per run) wraps `lmfilament::Texture`: 8-bit RGBA, row 0 = top = `v = 0`, filter (`linear` = trilinear, `nearest`), wrap (`repeat`, `clamp`, `mirror`) and a preview-only UV `repeat`. `Material.set_texture(name, tex)` only records the pair; `Scene.add*` checks the parameter exists, is a `sampler2d`, uploads a `filament::Texture` (RGBA8, or SRGB8_A8 for `albedo`/`baseColor`/`color`/`emissive`, with `GEN_MIPMAPPABLE` and generated mips), binds it with a `TextureSampler` built from the filter and wrap at that moment, and gives the preview a pointer to the albedo image. `Material.set_float` on a sampler name is an error. The preview treats a material whose source says `shadingModel : unlit` as unlit (albedo drawn as is). Primitive UVs: planes and box faces are upright when viewed from outside (top face with -Z up), spheres/cylinders/cones/capsules unroll with u growing left to right, cap rings are projected from above; `mesh_obj` flips `vt` v. Text meshes have no UVs, so texturing one is an error.

## Scene graph

`Scene.node()` creates an entity with a `TransformManager` component; `Part`s (`Scene.add_part`) and child `Node`s attach with
`parent(node)`, which calls `TransformManager::setParent`, so Filament composes the chain. The preview renderer keeps its own
`XForm` per part/node (`PreviewScene::nodes`, parent = node index) and composes the same chain; `XForm` is the source of truth and
`sync()` pushes it into Filament after every setter. Normals use the cofactor matrix (non-uniform scale safe). Cycles are rejected.
Material parameters are snapshotted at `add_part`, so one `Material` can be re-colored between adds.

`Scene.shadows(true)` makes the preview renderer build a 2048x2048 orthographic shadow map for the first directional light
(slope-scaled bias, 3x3 PCF) over all world-space triangles; it only darkens that light's contribution. Filament itself is unaffected.

To add an API: edit the schema, register the matching method with identical parameter/return type
strings, rebuild, and check `diff <(lmscript --dsl-signatures schema/lmfilament.dsl.json) <(build/host/lmfilament --dsl-signatures)`
(`test.sh` does this).
