# The four 3D hosts: how they relate and how to work on them

Identical copy in `durable-memory/20-sibling-3d-hosts-and-conventions.md` of lmblender, lmcad, lmmagnum and lmfilament. Change the
facts here in all four in the same task. Each repo's own docs cover its internals; this file covers everything that is true across
the family, plus the working conventions every agent must follow.

## 1. The family

Four static, dependency-free C++ hosts for lmscript `_dsl_` blocks (cpp-dsl-sdk `dslvm.hpp`, DSL bytecode v5). Each embeds its JSON
schema, binds methods in `register_host()`, and prints one JSON result line (`{"ok": true, ...}`; exit 0 ok, 2 usage, 3 load/run error).

| | lmblender | lmcad | lmmagnum | lmfilament |
| --- | --- | --- | --- | --- |
| engine | Blender Cycles (CPU, Embree, OIDN) | OpenCASCADE B-rep (solids, booleans, fillets) | Magnum Math + Primitives | Google Filament 1.78.1 (noop backend) + preview rasterizer |
| pixels from | Cycles path tracing (real GI, shadows, glass) | own CPU rasterizer | own CPU rasterizer (+ shadow map) | lmfilament's own CPU preview rasterizer (+ shadow map) |
| up axis | **Z** | Y | Y | Y |
| camera | `camera_look_at(ex,ey,ez, tx,ty,tz, fov)`, fov vertical | `camera(fov, ex,ey,ez, tx,ty,tz)`, fov vertical | `camera(fov, ex,ey,ez, tx,ty,tz)`, fov **horizontal** | `camera(fov, ex,ey,ez, tx,ty,tz)`, fov vertical |
| rotation units | degrees (`rotate_x/y/z`, baked into the mesh) | **radians** (`rotate(rx,ry,rz)`) | degrees (`rotate`, Euler Z,Y,X) | degrees (`rotate`, Euler Z,Y,X) |
| materials | `scene.material(r,g,b,metal,rough,transmission,er,eg,eb,estrength)` | `material().color().roughness().specular().shininess()` | same as lmcad | one compiled Filament material, `set_float3("baseColor", ...)` before each add |
| license | binary GPL-3.0-or-later, own sources MIT | MIT | MIT | MIT |
| run it | released host via lmscript's tool cache (no local build on the dev box) | released host via tool cache | `lmmagnum/build/lmmagnum` | `lmfilament/build/host/lmfilament` (needs `~/toolchains/llvm19`, clang >= 17) |

`lmscript X.lms` compiles the outer script, which compiles the `_dsl_` block against the host named by `cli=`/`install=`, then runs
the host. Put a local build directory first on `PATH` to test unreleased host changes (`dsl_cli` finds it before the tool cache
`~/.cache/lmscript/tools/github/lmctl-ai/<tool>/latest`).

## 2. DSL cheat sheet (same scene, four spellings)

| thing | lmblender | lmcad | lmmagnum | lmfilament |
| --- | --- | --- | --- | --- |
| sphere | `let o = scene.uv_sphere(r,48,32,true); o.translate(x,y,z); o.set_material(m);` | `scene.sphere(r, m).position(x,y,z);` | `scene.sphere(r, m).translate(x,y,z);` | `s.add_at(fil.sphere(r,32), mat, x,y,z);` |
| box | `scene.cube(1.0)` then `scale(w,d,h)` | `scene.box(w,h,d, m)` | `scene.box(w,h,d, m)` | `fil.box(w,h,d)` |
| cylinder / cone | `scene.cylinder(r,h,32,true)` / `scene.cone(r,h,32,true)` | `scene.cylinder(r,h,m)`; cone via `revolve_y(sketch)` | `scene.cylinder(r,h,m)` / `scene.cone(r,h,m)` | `fil.cylinder(r,h,seg)` / `fil.cone(r,h,seg)` |
| tube between points | `scene.tube_between(x1,y1,z1,x2,y2,z2,r).set_material(m)` | `scene.tube_between(x1,..,z2, r, m)` | same as lmcad | `s.add(fil.tube_between(x1,..,z2, r), mat)` |
| per-frame animation | `keyframe_transform`, `camera_keyframe`, `render_sequence` (<= 60 frames per call at 640x360) | mutate (`position`, `visible`) then `render_png` per frame | same as lmcad (`translate`, `visible`) | `Part.translate/visible`, then `render_png` per frame |
| lights | `area_light`, `sun_light(dir,...)`, `world_color`, `ground_plane` | `directional_light(dx,dy,dz,i)`, `fill_light`, `ambient`, `background` | `directional_light`, `ambient`, `background`, `shadows(true)` | `directional_light(r,g,b,lux,dx,dy,dz)` (110000 = full), `point_light`, `skybox`, `shadows(true)` |
| render | `render_png(path, samples, w, h)` | `render_png(path, w, h)` | `render_png(path, w, h)` | `render_png(path, w, h)` (plus `render_frames(n)` for Filament itself) |

Language limits inside `_dsl_`: no arrays, no `assert`/`println` (check in the outer script), int/int is integer division, loops are
`for (i in range(a,b))`, `//` and `#` comments both work, strings build with `+` and `str()`. Variables holding host objects can be
mutated later (that is how animation works).

## 3. Porting an example between repos (what went wrong before)

1. Keep the source's data and logic; swap only the API calls using the table above. Most ports were done with throwaway generator
   scripts that substitute calls; none are kept, the recipe is this section.
2. Axes: Z-up (lmblender) -> Y-up: `(x,y,z) -> (x,z,-y)` and the same map for camera eye/target keeps the view. Back: `(x,y,z) -> (x,-z,y)`.
3. Units: lmcad rotations are radians, everyone else degrees. In lmcad rotate first, then `translate` (position then rotate pivots about the origin).
4. Field of view: lmmagnum is horizontal, the others vertical. At 16:9 a vertical 43 degrees is a horizontal ~70.
5. lmcad has no cone/capsule/torus/triangle primitive (use `revolve_y`/`extrude` of a `sketch`); lmblender has no triangle primitive; its `label()` accepts letters and spaces only.
6. No global illumination or reflections outside lmblender: Cornell box colour bleeding and mirror spheres only exist there. State that in the example header and the README caption.
7. lmfilament binds one material snapshot per `add`: set `baseColor` before each add. Parameters are validated on add.
8. Keep examples self-contained so `lmctl autorun <raw URL>` works from any directory: inline materials, write only to `images/` and `build/`.
9. Verify the picture: render, then look at it. Cameras, fov and light intensities always need one tuning pass per engine.

## 4. Examples shared by two or more repos

| example | lmblender | lmcad | lmmagnum | lmfilament |
| --- | --- | --- | --- | --- |
| `ball-stick` | x | x | x | x |
| `buckyball` | x | x | x | x |
| `bullet` | x | x | x | x |
| `caffeine-molecule` | x | x | x | x |
| `cornell-box` | x | x | x | x |
| `cubemap` | x | x | x | x |
| `depthtesting` |   |   | x | x |
| `dna-double-helix` | x | x | x | x |
| `fluidsimulation2d` | x | x | x | x |
| `fluidsimulation3d` | x | x | x | x |
| `level-1-sessions` | x | x | x | x |
| `level-2-orchestration` | x | x | x | x |
| `level-3-hands-off-lead` | x | x | x | x |
| `level-4-automation` | x | x | x | x |
| `lightbulb` |   |   | x | x |
| `material-sandbox` | x | x | x |   |
| `model-viewer` | x | x | x | x |
| `multidraw` | x | x | x | x |
| `pbr-spheres` | x | x | x |   |
| `primitives` |   | x | x | x |
| `primitives-scene-graph` |   |   | x | x |
| `saturn-rings` | x | x | x | x |
| `shadows` | x |   | x | x |
| `text` | x |   | x | x |
| `textured-triangle` |   | x | x |   |
| `truss-bridge` | x | x | x | x |

Equivalents under other names: lmfilament `material_sandbox` = `material-sandbox`; lmblender `primitives-gallery` ~ `primitives`.
Everything else is repo-specific (lmblender 73 examples incl. feature regressions, lmcad B-rep engineering stories, lmmagnum Magnum
sample analogues, lmfilament Filament sample ports). Origin of the shared scenes: caffeine, DNA, Cornell box, Saturn from lmblender;
water, C60, truss from lmcad; roughness sweep, material grid, model viewer, text, primitives, shadows, multidraw, cubemap, bullet,
fluids from lmmagnum/lmfilament (Magnum examples); the four `level-N-*` clips are the lmctl.com homepage storyboards (source:
`lmctl-ai/lmctl` `scripts/homepage/video`, lmcad and lmblender variants).

## 5. README gallery conventions

- Pictures sit in 3-column tables; a caption is one line, the origin in parentheses, then `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/<repo>/main/examples/<name>.lms`.
- Images live in `docs/images/` (PNG; GIF for animation), MP4s in `docs/videos/`. Budgets: lmcad PNG <= 300,000 bytes, lmblender PNG <= 300 KiB, GIF <= 3 MB (both repos' tests check README links, magic bytes and sizes).
- Regenerate: lmmagnum `scripts/update-readme-images.sh`; lmfilament `examples/render-all.sh`; lmcad `examples/render-all.sh` (needs ffmpeg); lmblender `examples/render-all.sh`. Examples write `images/<name>.png` in the current directory, so run them from a scratch directory.
- Animated examples write 240 frames to `build/<name>/frame-N.png`, then `ffmpeg` (optional; the example skips with a message when absent) encodes `images/<name>.mp4` and a 12 fps palette GIF.
- Add the picture, the caption, the test list entry (`test.sh` image loop) and the docs row in the same commit as the example.

## 6. Working conventions (mandatory)

- **Always commit and push to `origin main` at the end of a task**, without asking and without explaining why something is unpushed. Rebase on a rejected push; never force-push `main`.
- **Identity:** `mike-lmctl <281311770+mike-lmctl@users.noreply.github.com>` (`git config user.name/user.email`). Never use `mikema3`. End commit messages with the Co-Authored-By line the harness provides.
- **Releases are automatic.** Every push to `main` that CI accepts publishes `vMAJOR.MINOR.<run number>` (see the "Automatic releases" section of each repo's CI doc). Do not bump versions or cut tags by hand; MAJOR.MINOR changes only when the version source (`CMakeLists.txt` `project(... VERSION)`, or lmblender's `VERSION` file) is edited deliberately. `@latest` lags a push by the CI time (20-40 min cold); that gap is accepted.
- **Docs travel with code.** Update `durable-memory/` (and this file when a fact about the family changes) in the same commit as the behaviour change.
- **Tests before pushing:** lmmagnum and lmfilament `./test.sh` (minutes); lmcad `test.sh` needs the OCCT prefix and a full build, so rely on CI plus running the touched examples against the released host; lmblender's full `test.sh` needs a complete Cycles build (hours), so compile-check examples with `lmscript` and read the README gate in `test.sh`.
- **Do not trust workflow-level green** where jobs are `continue-on-error` (lmblender `platforms`): read per-job conclusions.
- ffmpeg 4.4 is installed locally; PIL is available for contact sheets when reviewing many pictures.
