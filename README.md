# lmfilament

> The public README, docs, images, and examples for `lmfilament` are MIT under `LICENSE-examples`. Binary releases use `LICENSE`; implementation source and its history are maintained privately in `lmctlhq/lmfilament`.

**Drive [Google Filament](https://github.com/google/filament) from LMScript with one static executable.**

lmfilament is a native host for the lmscript `_dsl_` statement. It embeds Filament 1.78.1 (material compiler,
engine, geometry tools) and exposes it through `private implementation detail`,
the same way [lmhtml](https://github.com/lmctl-ai/lmhtml) and [lmcad](https://github.com/lmctl-ai/lmcad) do.
The binary has no runtime dependency.

```lms
p = _lmcode_ cli="lmfilament" install="github:lmctl-ai/lmfilament@latest";
let mat = fil.material_file("private implementation detail");
mat.set_float3("baseColor", 0.8, 0.8, 0.8);
mat.set_float("metallic", 1.0);
let s = fil.scene();
s.add_at(fil.mesh_obj("private implementation detail"), mat, 0.0, 0.0, -4.0);
s.directional_light(0.9551, 0.8276, 0.7678, 110000.0, 0.7, -1.0, -0.8);
s.render_frames(2);
end _lmcode_
```

## Install lmctl

Everything in this repository runs through [lmctl](https://lmctl.com), the core tool of the lmctl family. Install it once, then run any `.lms` file or raw URL:

```bash
npm install -g @lmctl-ai/lmctl
lmctl autorun path/to/example.lms
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/example.lms
```

## What it does (and does not)

Real: `.mat` -> `.filamat` compilation (all upstream sample materials), Filament engine/scene/camera/light/
renderable/material-instance objects, procedural meshes, OBJ import/export, tangent frames.
Not real: **Filament pixels**. A fully static binary cannot load a GPU driver, so Filament runs on its noop backend
and `render_frames` drives the render loop without output. The pictures below come from lmfilament's own small CPU
preview rasterizer (`Scene.render_png`): Lambert shading from the scene's lights, `baseColor` from the material
parameters (or the albedo texture), unlit vertex colors and an optional shadow map. It is a preview, not Filament's PBR renderer.
See private implementation detail

## Examples

Each picture is produced by a self-contained `.lms` port of an upstream Filament C++ sample of a [Magnum Python example](https://doc.magnum.graphics/python/examples/), or of an [lmblender](https://github.com/lmctl-ai/lmblender) / [lmcad](https://github.com/lmctl-ai/lmcad) scene in
`examples/` (materials are inlined, so the `lmctl autorun` command in each caption runs from any
directory and writes `images/<name>.png`). `examples/render-all.sh` regenerates all of them; the two Suzanne examples
read the OBJ (and `suzanne`/`texturedquad` their PNGs) from a Filament checkout (`private implementation detail`).
`private implementation detail` says what each port keeps.
`strobecolor`, `point_sprites` and `compile-sample-materials` produce no picture.

| Vertex-colored triangle | Suzanne, PBR `aiDefaultMat` | Suzanne, `texturedLit` |
| --- | --- | --- |
| <img src="docs/images/hellotriangle.png" width="290" alt="Vertex-colored triangle"> | <img src="docs/images/hellopbr.png" width="290" alt="Suzanne, PBR aiDefaultMat"> | <img src="docs/images/suzanne.png" width="290" alt="Suzanne, texturedLit"> |
| Upstream hellotriangle: unlit `bakedColor` material<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/hellotriangle.lms` | Metallic 1, roughness 0.4, sun light (needs a clone for the OBJ)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/hellopbr.lms` | Same scene with the textured material (five monkey images bound to its samplers; the preview shades with the albedo; needs a clone)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/suzanne.lms` |

| Primitives | Primitives, using a scene graph | Model viewer |
| --- | --- | --- |
| <img src="docs/images/primitives.png" width="290" alt="Primitives"> | <img src="docs/images/primitives-scene-graph.png" width="290" alt="Primitives, using a scene graph"> | <img src="docs/images/model-viewer.png" width="290" alt="Model viewer"> |
| Magnum Python example: box, sphere, icosphere, cylinder, cone, capsule<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/primitives.lms` | Magnum Python example: parts parented to nested `Node`s (Filament TransformManager)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/primitives-scene-graph.lms` | Magnum Python example: a posed robot under one scene-graph node<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/model-viewer.lms` |

| Text | Depth testing | Vertex buffer layout |
| --- | --- | --- |
| <img src="docs/images/text.png" width="290" alt="Text"> | <img src="docs/images/depthtesting.png" width="290" alt="Depth testing"> | <img src="docs/images/vbotest.png" width="290" alt="Vertex buffer layout"> |
| Magnum Python example: voxel text from a 5x7 font<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/text.lms` | Two triangles at different depths; the nearer one wins<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/depthtesting.lms` | Position + packed ubyte4 color, `bakedColor`<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/vbotest.lms` |

| Blue skybox view | Sun + five point lights | Ground plane and sun |
| --- | --- | --- |
| <img src="docs/images/viewtest.png" width="290" alt="Blue skybox view"> | <img src="docs/images/lightbulb.png" width="290" alt="Sun + five point lights"> | <img src="docs/images/shadowtest.png" width="290" alt="Ground plane and sun"> |
| Triangle on a pure blue skybox<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/viewtest.lms` | Metal sphere over a ground plane at the upstream light positions<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/lightbulb.lms` | `groundShadow` plane and a lit sphere on a sky-blue skybox<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/shadowtest.lms` |

| Material sandbox | Textured quad | Heightfield plane |
| --- | --- | --- |
| <img src="docs/images/material_sandbox.png" width="290" alt="Material sandbox"> | <img src="docs/images/texturedquad.png" width="290" alt="Textured quad"> | <img src="docs/images/heightfield.png" width="290" alt="Heightfield plane"> |
| Nine sandbox materials on spheres (lit, fade, transparent, refraction, cloth, ...)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/material_sandbox.lms` | `bakedTexture` quad with upstream's Moss_01 image (needs a clone for the PNG)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/texturedquad.lms` | `heightfield` material on a quad (height texture not wired yet)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/heightfield.lms` |

| Caffeine molecule | Water molecule | Buckminsterfullerene C60 |
| --- | --- | --- |
| <img src="docs/images/caffeine-molecule.png" width="290" alt="Caffeine molecule"> | <img src="docs/images/ball-stick.png" width="290" alt="Water molecule"> | <img src="docs/images/buckyball.png" width="290" alt="Buckminsterfullerene C60"> |
| Ball-and-stick, 24 atoms in CPK colours (from lmblender)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/caffeine-molecule.lms` | 104.5 degree H-O-H angle (from lmcad)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/ball-stick.lms` | 60 atoms from icosahedral symmetry, 90 bonds (from lmcad)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/buckyball.lms` |

| B-DNA double helix | Cornell box | Saturn and its rings |
| --- | --- | --- |
| <img src="docs/images/dna-double-helix.png" width="290" alt="B-DNA double helix"> | <img src="docs/images/cornell-box.png" width="290" alt="Cornell box"> | <img src="docs/images/saturn-rings.png" width="290" alt="Saturn and its rings"> |
| 28 base pairs, 144 degree groove geometry (from lmblender)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/dna-double-helix.lms` | Red and green walls, two white blocks (from lmblender)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/cornell-box.lms` | Ring radii to scale, Cassini Division (from lmblender)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/saturn-rings.lms` |

| Pratt truss footbridge | Shadows | Multi draw |
| --- | --- | --- |
| <img src="docs/images/truss-bridge.png" width="290" alt="Pratt truss footbridge"> | <img src="docs/images/shadows.png" width="290" alt="Shadows"> | <img src="docs/images/multidraw.png" width="290" alt="Multi draw"> |
| Method of joints: red tension, blue compression (from lmcad)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/truss-bridge.lms` | Floor and four solids with a shadow map<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/shadows.lms` | 180 meshes of four kinds in six rings<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/multidraw.lms` |

| Cube map | Bullet (analytic) | 3D fluid flow |
| --- | --- | --- |
| <img src="docs/images/cubemap.png" width="290" alt="Cube map"> | <img src="docs/images/bullet.png" width="290" alt="Bullet (analytic)"> | <img src="docs/images/fluidsimulation3d.png" width="290" alt="3D fluid flow"> |
| Six-face room around glossy spheres<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/cubemap.lms` | 64 boxes falling and bouncing, closed-form ballistics<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/bullet.lms` | 32 ABC-flow streamlines coloured by speed<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/fluidsimulation3d.lms` |

| 2D fluid flow |   |   |
| --- | --- | --- |
| <img src="docs/images/fluidsimulation2d.png" width="290" alt="2D fluid flow"> |  |  |
| A counter-rotating vortex pair, 40 streamlines<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/fluidsimulation2d.lms` |   |   |

### Homepage clips from lmctl.com

The four 10 s clips on the [lmctl.com](https://lmctl.com) homepage, one per level of the agent-automation ladder. Their scene scripts live in lmctl-ai/lmctl; the published videos add lmdraw title and label overlays on top of the 3D scenes shown here. Each example writes 240 frames, then ffmpeg encodes `images/<name>.mp4` and `.gif`; the lmfilament version is a port of the lmcad storyboard (same animation, preview rasterizer, Filament scene graph).

| Level 1: names and teams | Level 2: agents talk through a Lead |
| --- | --- |
| <img src="docs/images/level-1-sessions.gif" width="440" alt="Level 1: names and teams"> | <img src="docs/images/level-2-orchestration.gif" width="440" alt="Level 2: agents talk through a Lead"> |
| Sessions gather into Lead, Coder and Review rows; a lens inspects one and a usage gauge rises<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/level-1-sessions.lms`<br>[MP4](docs/videos/level-1-sessions.mp4) | Provider-coloured agents orbit the Lead; message packets travel through it<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/level-2-orchestration.lms`<br>[MP4](docs/videos/level-2-orchestration.mp4) |

| Level 3: a hands-off Lead | Level 4: automation |
| --- | --- |
| <img src="docs/images/level-3-hands-off-lead.gif" width="440" alt="Level 3: a hands-off Lead"> | <img src="docs/images/level-4-automation.gif" width="440" alt="Level 4: automation"> |
| A requirement and its checklist enter; a reviewed result returns<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/level-3-hands-off-lead.lms`<br>[MP4](docs/videos/level-3-hands-off-lead.mp4) | A brainstorm becomes a Work, Review, QA and Acceptance pipeline<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/level-4-automation.lms`<br>[MP4](docs/videos/level-4-automation.mp4) |


### Textures

`Filament.texture(w, h)`, `texture_checker(...)` and `texture_file(path)` (PNG, JPEG, BMP, TGA) create images; `Texture.pixel`, `filter`, `wrap`, `repeat` and `save_png` shape and save them; `Material.set_texture("albedo", tex)` binds one to a `sampler2d` parameter. The image is uploaded to Filament (noop backend) when the material is added to a scene with `Scene.add`/`add_at`/`add_part`, so set filtering and wrapping before that. The CPU preview samples the albedo image (the sampler named `albedo`, `baseColor`, `color` or `emissive`, else the first one) with mip-mapped trilinear or nearest filtering; other maps (roughness, metallic, normal, ao) are uploaded but not used for shading. Meshes carry UVs with `v = 0` at the top row of the image (OBJ `vt` is flipped on import).

| Textured planet | Textured primitives | Texture filtering and wrapping |
| --- | --- | --- |
| <img src="docs/images/textured-planet.gif" width="290" alt="Textured planet"> | <img src="docs/images/textured-primitives.png" width="290" alt="Textured primitives"> | <img src="docs/images/texture-modes.png" width="290" alt="Texture filtering and wrapping"> |
| A generated Earth map and a cratered moon on lit spheres, 36-frame turn (GIF via ffmpeg)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/textured-planet.lms` | One brick image on box, sphere, cylinder, cone, capsule, tube and ring, with shadows<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/textured-primitives.lms` | Linear vs nearest, mip filtering at a grazing angle, repeat, clamp and mirror<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/texture-modes.lms` |

## Build

Needs clang >= 17, cmake, ninja, python3 and `lmscript` (npm `@lmctl-ai/lmctl`).
Details in `private implementation detail`.

```sh
private implementation detail      # shallow checkout at filament.lock
private implementation detail     # Filament static libs
private implementation detail              # private implementation detail
./test.sh
```

## Downloads

The current release lane publishes Linux x64 and arm64 binaries (glibc >= 2.35)
on the [releases page](https://github.com/lmctl-ai/lmfilament/releases). Earlier
releases may contain macOS assets; macOS and Windows are not produced by the
current lane.

## License


Releases up to and including v0.3.20 were published under the MIT License (see the LICENSE asset of that release). Releases after v0.3.20 are published under the Lattice Systems Binary License 1.0 (this file).
The README, docs, images, and `examples/*.lms` are MIT-licensed under `LICENSE-examples`.
Binary releases are licensed under `LICENSE`. Third-party components remain under their own licenses and notices.
