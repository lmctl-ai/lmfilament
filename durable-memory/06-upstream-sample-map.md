# Upstream sample -> example map

| upstream (`third_party/filament/samples/`) | example | fidelity |
| --- | --- | --- |
| `hellotriangle.cpp` | `hellotriangle.lms` | geometry, colors, material, skybox, camera as upstream |
| `vbotest.cpp` | `vbotest.lms` | same triangle/material; interleaved-VBO layout not expressible |
| `viewtest.cpp` | `viewtest.lms` | scene only; multi-view not expressible |
| `strobecolor.cpp` | `strobecolor.lms` | skybox only; per-frame animation not expressible |
| `depthtesting.cpp` | `depthtesting.lms` | two depth-separated triangles |
| `hellopbr.cpp` | `hellopbr.lms` | monkey.obj, aiDefaultMat parameters, sun light as upstream |
| `suzanne.cpp` | `suzanne.lms` | geometry/material/light; PNG textures missing |
| `shadowtest.cpp` | `shadowtest.lms` | ground + sphere + sun; shadow options not exposed |
| `lightbulb.cpp` | `lightbulb.lms` | sphere stand-in for the bulb model; spot lights as point lights |
| `heightfield.cpp` | `heightfield.lms` | material compiles; plane stand-in, height texture missing |
| `texturedquad.cpp` | `texturedquad.lms` | quad + bakedTexture; texture missing |
| `point_sprites.cpp` | `point_sprites.lms` | material only |
| `material_sandbox.cpp` | `material_sandbox.lms` | all sandbox materials on spheres |
| `materials/*.mat` | `compile-sample-materials.lms` | all 28 compile to `build/materials/*.filamat` |

Each example's header comment states what is omitted. Every example prints `ok NAME` on success;
`test.sh` requires that line.

Pictures: `examples/render-all.sh` runs every example (each writes `images/NAME.png` via `Scene.render_png`, the CPU
preview) and copies them to `docs/images/` for the README. Materials are inlined into the `.lms` files so the
raw-URL `lmctl autorun` commands work from any directory; only the Suzanne examples need the Filament checkout.
