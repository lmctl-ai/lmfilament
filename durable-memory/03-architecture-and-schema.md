# Architecture and schema

## Source layout

| file | role |
| --- | --- |
| `schema/lmfilament.dsl.json` | the contract: types, methods, context `fil: Filament` |
| `cpp/dslvm.hpp` | vendored SDK header (do not edit; `sdk.lock`) |
| `cpp/main.cpp` | `Host` arenas, Filament glue, method registration, CLI |
| `cpp/mesh.{hpp,cpp}` | procedural meshes, OBJ read/write, tangent frames |
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

## Schema v0.2 surface

```
Filament: version() material(name, source) material_file(path) box(x,y,z) sphere(r, segments)
          plane(w, d) triangle() mesh_obj(path) scene()
Material: name() package_size() save(path) set_float(n,v) set_float3(n,x,y,z) set_float4(n,x,y,z,w)
Mesh:     vertex_count() triangle_count() has_tangents() has_colors() compute_tangents() save_obj(path)
Scene:    add(mesh, mat) add_at(mesh, mat, x,y,z) skybox(r,g,b) camera(fov,ex,ey,ez,tx,ty,tz)
          sun(intensity) directional_light(r,g,b,intensity,dx,dy,dz) point_light(x,y,z,intensity,falloff)
          render_frames(n) renderable_count() light_count()
```

To add an API: edit the schema, register the matching method with identical parameter/return type
strings, rebuild, and check `diff <(lmscript --dsl-signatures schema/lmfilament.dsl.json) <(build/host/lmfilament --dsl-signatures)`
(`test.sh` does this).
