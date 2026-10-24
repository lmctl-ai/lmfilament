# Upstream sample -> example map

| upstream (`third_party/filament/samples/`) | example | fidelity |
| --- | --- | --- |
| `hellotriangle.cpp` | `hellotriangle.lms` | geometry, colors, material, skybox, camera as upstream |
| `vbotest.cpp` | `vbotest.lms` | same triangle/material; interleaved-VBO layout not expressible |
| `viewtest.cpp` | `viewtest.lms` | scene only; multi-view not expressible |
| `strobecolor.cpp` | `strobecolor.lms` | skybox only; per-frame animation not expressible |
| `depthtesting.cpp` | `depthtesting.lms` | two depth-separated triangles |
| `hellopbr.cpp` | `hellopbr.lms` | monkey.obj, aiDefaultMat parameters, sun light as upstream |
| `suzanne.cpp` | `suzanne.lms` | geometry/material/light; the five monkey PNGs bound to the sampler parameters (preview shades with the albedo only) |
| `shadowtest.cpp` | `shadowtest.lms` | ground + sphere + sun; shadow options not exposed |
| `lightbulb.cpp` | `lightbulb.lms` | sphere stand-in for the bulb model; spot lights as point lights |
| `heightfield.cpp` | `heightfield.lms` | material compiles; plane stand-in (the height texture is read in the vertex stage, which the preview does not run) |
| `texturedquad.cpp` | `texturedquad.lms` | quad + bakedTexture with the upstream Moss_01 colour image |
| `point_sprites.cpp` | `point_sprites.lms` | material only |
| `material_sandbox.cpp` | `material_sandbox.lms` | all sandbox materials on spheres |
| `materials/*.mat` | `compile-sample-materials.lms` | all 28 compile to `build/materials/*.filamat` |
| Magnum Python `Primitives` | `primitives.lms` | all primitives the host builds; no mouse input |
| Magnum Python `Primitives, using a scene graph` | `primitives-scene-graph.lms` | nested nodes via TransformManager |
| Magnum Python `Model viewer` | `model-viewer.lms` | robot from primitives under one node; no importer UI |
| Magnum Python `Text` | `text.lms` | voxel 5x7 font instead of distance-field glyphs |
| lmblender `caffeine-molecule` | `caffeine-molecule.lms` | atoms/bonds copied; tubes via `fil.tube_between` |
| lmcad `ball-stick` | `ball-stick.lms` | water molecule |
| lmcad `buckyball` | `buckyball.lms` | C60 logic copied verbatim; asserts dropped |
| lmblender `dna-double-helix` | `dna-double-helix.lms` | strands as chains of tubes |
| lmblender `cornell-box` | `cornell-box.lms` | no global illumination (preview shading only) |
| lmblender `saturn-rings` | `saturn-rings.lms` | `fil.ring` bands, no planet shadow |
| lmcad `truss-bridge` | `truss-bridge.lms` | 3D part only; member colour/radius from the closed-form forces |
| lmmagnum `texture-modes` | `texture-modes.lms` | filtering, mip levels, wrap modes through sampler parameters |
| lmmagnum `textured-planet` | `textured-planet.lms` | generated Earth/moon maps on lit spheres, 36-frame GIF |
| lmmagnum `textured-primitives` | `textured-primitives.lms` | brick image on every primitive, shadow map |
| magnum-examples `shadows` | `shadows.lms` | preview shadow map (`scene.shadows(true)`) instead of Filament's cascaded maps |
| magnum-examples `multidraw` | `multidraw.lms` | 180 meshes, each its own renderable |
| magnum-examples `cubemap` | `cubemap.lms` | six-face room + spheres; no reflections |
| magnum-examples `bullet` | `bullet.lms` | closed-form ballistics, no rigid-body engine |
| magnum-examples `fluidsimulation3d` / `fluidsimulation2d` | `fluidsimulation3d.lms`, `fluidsimulation2d.lms` | streamline visualisation, no solver |
| lmctl.com homepage clips | `level-1-sessions.lms` ... `level-4-automation.lms` | ported from the lmcad storyboards; `Part.visible` toggles parts per frame; ffmpeg writes MP4 + GIF |

Each example's header comment states what is omitted. Every example prints `ok NAME` on success;
`test.sh` requires that line.

Pictures: `examples/render-all.sh` runs every example (each writes `images/NAME.png` via `Scene.render_png`, the CPU
preview) and copies them to `docs/images/` for the README. Materials are inlined into the `.lms` files so the
raw-URL `lmctl autorun` commands work from any directory; only the Suzanne examples need the Filament checkout.
