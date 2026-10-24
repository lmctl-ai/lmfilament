# lmfilament

**Drive [Google Filament](https://github.com/google/filament) from LMScript with one static executable.**

lmfilament is a native host for the lmscript `_dsl_` statement. It embeds Filament 1.78.1 (material compiler,
engine, geometry tools) and exposes it through [`schema/lmfilament.dsl.json`](schema/lmfilament.dsl.json),
the same way [lmhtml](https://github.com/lmctl-ai/lmhtml) and [lmcad](https://github.com/lmctl-ai/lmcad) do.
The binary has no runtime dependency.

```lms
p = _dsl_ cli="lmfilament" install="github:lmctl-ai/lmfilament@latest";
let mat = fil.material_file("third_party/filament/samples/materials/aiDefaultMat.mat");
mat.set_float3("baseColor", 0.8, 0.8, 0.8);
mat.set_float("metallic", 1.0);
let s = fil.scene();
s.add_at(fil.mesh_obj("third_party/filament/assets/models/monkey/monkey.obj"), mat, 0.0, 0.0, -4.0);
s.directional_light(0.9551, 0.8276, 0.7678, 110000.0, 0.7, -1.0, -0.8);
s.render_frames(2);
end _dsl_
```

## What it does (and does not)

Real: `.mat` -> `.filamat` compilation (all upstream sample materials), Filament engine/scene/camera/light/
renderable/material-instance objects, procedural meshes, OBJ import/export, tangent frames.
Not real: **Filament pixels**. A fully static binary cannot load a GPU driver, so Filament runs on its noop backend
and `render_frames` drives the render loop without output. The pictures below come from lmfilament's own small CPU
preview rasterizer (`Scene.render_png`): Lambert shading from the scene's lights, `baseColor` from the material
parameters, unlit vertex colors, no textures, no shadows. It is a preview, not Filament's PBR renderer.
See [durable-memory/](durable-memory/).

## Examples

Each picture is produced by a self-contained `.lms` port of an upstream Filament C++ sample or of a [Magnum Python example](https://doc.magnum.graphics/python/examples/) in
[`examples/`](examples) (materials are inlined, so the `lmctl autorun` command in each caption runs from any
directory and writes `images/<name>.png`). `examples/render-all.sh` regenerates all of them; the two Suzanne examples
read the OBJ from a Filament checkout (`scripts/sync-filament.sh`).
[`durable-memory/06-upstream-sample-map.md`](durable-memory/06-upstream-sample-map.md) says what each port keeps.
`strobecolor`, `point_sprites` and `compile-sample-materials` produce no picture.

| Vertex-colored triangle | Suzanne, PBR `aiDefaultMat` | Suzanne, `texturedLit` |
| --- | --- | --- |
| <img src="docs/images/hellotriangle.png" width="290"> | <img src="docs/images/hellopbr.png" width="290"> | <img src="docs/images/suzanne.png" width="290"> |
| Upstream hellotriangle: unlit `bakedColor` material<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/hellotriangle.lms` | Metallic 1, roughness 0.4, sun light (needs a clone for the OBJ)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/hellopbr.lms` | Same scene with the textured material (textures not wired yet; needs a clone)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/suzanne.lms` |

| Primitives | Primitives, using a scene graph | Model viewer |
| --- | --- | --- |
| <img src="docs/images/primitives.png" width="290"> | <img src="docs/images/primitives-scene-graph.png" width="290"> | <img src="docs/images/model-viewer.png" width="290"> |
| Magnum Python example: box, sphere, icosphere, cylinder, cone, capsule<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/primitives.lms` | Magnum Python example: parts parented to nested `Node`s (Filament TransformManager)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/primitives-scene-graph.lms` | Magnum Python example: a posed robot under one scene-graph node<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/model-viewer.lms` |

| Text | Depth testing | Vertex buffer layout |
| --- | --- | --- |
| <img src="docs/images/text.png" width="290"> | <img src="docs/images/depthtesting.png" width="290"> | <img src="docs/images/vbotest.png" width="290"> |
| Magnum Python example: voxel text from a 5x7 font<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/text.lms` | Two triangles at different depths; the nearer one wins<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/depthtesting.lms` | Position + packed ubyte4 color, `bakedColor`<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/vbotest.lms` |

| Blue skybox view | Sun + five point lights | Ground plane and sun |
| --- | --- | --- |
| <img src="docs/images/viewtest.png" width="290"> | <img src="docs/images/lightbulb.png" width="290"> | <img src="docs/images/shadowtest.png" width="290"> |
| Triangle on a pure blue skybox<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/viewtest.lms` | Metal sphere over a ground plane at the upstream light positions<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/lightbulb.lms` | `groundShadow` plane and a lit sphere on a sky-blue skybox<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/shadowtest.lms` |

| Material sandbox | Textured quad | Heightfield plane |
| --- | --- | --- |
| <img src="docs/images/material_sandbox.png" width="290"> | <img src="docs/images/texturedquad.png" width="290"> | <img src="docs/images/heightfield.png" width="290"> |
| Nine sandbox materials on spheres (lit, fade, transparent, refraction, cloth, ...)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/material_sandbox.lms` | `bakedTexture` quad (texture parameter not wired yet)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/texturedquad.lms` | `heightfield` material on a quad (height texture not wired yet)<br>`lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmfilament/main/examples/heightfield.lms` |

## Build

Needs clang >= 17, cmake, ninja, python3 and `lmscript` (npm `@lmctl-ai/lmctl`).
Details in [`durable-memory/02-build-run-test.md`](durable-memory/02-build-run-test.md).

```sh
scripts/sync-filament.sh      # shallow checkout at filament.lock
scripts/build-filament.sh     # Filament static libs
scripts/build.sh              # build/host/lmfilament
./test.sh
```

## Downloads

Release binaries for Linux (x64, arm64) and macOS (arm64, x64) are on the
[releases page](https://github.com/lmctl-ai/lmfilament/releases). Windows is not supported yet.

## License

MIT, see [LICENSE](LICENSE). Filament is Apache-2.0; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
