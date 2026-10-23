# Purpose and place

lmfilament is a native lmscript `_dsl_` host: one statically linked executable that embeds
[Google Filament](https://github.com/google/filament) 1.78.1 (pinned in `filament.lock`) and exposes a
small slice of it to LMScript. It follows the same shape as `../lmhtml` and `../lmcad`:

```
LMScript (.lms) --lmscript--> DSL bytecode (.dslb) --lmfilament --dsl--> Filament
                  schema/lmfilament.dsl.json  (SHA-256 bound to the bytecode)
```

- `../cpp-dsl-sdk` supplies `cpp/dslvm.hpp` (vendored, pinned by `sdk.lock`, refresh with `scripts/sync-sdk.sh`).
- The schema is embedded in the binary (`--dsl-schema --json` prints it byte for byte).
- The binary has no runtime dependency: `ldd build/host/lmfilament` says "not a dynamic executable".

## Scope: what this is and is not

Filament has no CPU renderer; its pixel-producing backends (OpenGL, Vulkan, Metal) load a GPU driver at
run time, which a fully static binary cannot do. lmfilament therefore builds Filament with the **noop
backend only**. What is real:

- Material compilation: `.mat` source -> `.filamat` through filamat + glslang + SPIRV-Tools + SPIRV-Cross
  (the same code as upstream `matc`), including all 28 upstream sample materials.
- Engine objects: `Engine`, `Scene`, `View`, `Camera`, `Renderer`, `SwapChain`, `Skybox`, `LightManager`,
  `RenderableManager`, `TransformManager`, vertex/index buffers, material instances and their parameters.
- Geometry: procedural meshes, OBJ import/export, tangent frames via `filament::geometry::SurfaceOrientation`.

What is not: Filament pixels. `Scene.render_frames(n)` drives `beginFrame/render/endFrame` against the noop driver
and returns the number of frames drawn. Pictures come from `Scene.render_png(path, w, h)`, lmfilament's own CPU preview rasterizer (`cpp/preview.cpp`):
z-buffer, near-plane clipping, 2x2 supersampling in linear light, Lambert shading from the scene's lights,
`baseColor`/`color` material parameter as albedo, unlit per-vertex colors, mild tone map. No textures, shadows,
specular or PBR; light units are mapped to a fixed preview exposure. It exists for documentation and smoke tests;
it is not Filament's renderer. See `05-open-items.md`.
