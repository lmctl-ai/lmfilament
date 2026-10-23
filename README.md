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
Not real: **pixels**. A fully static binary cannot load a GPU driver, so Filament runs on its noop backend and
`render_frames` drives the render loop without output. See [durable-memory/](durable-memory/).

## Examples

[`examples/`](examples) ports 14 upstream samples (`hellotriangle`, `hellopbr`, `suzanne`, `lightbulb`, ...).
[`durable-memory/06-upstream-sample-map.md`](durable-memory/06-upstream-sample-map.md) says what each keeps.

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
