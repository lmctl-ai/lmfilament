#!/usr/bin/env bash
# Verifies the static host and runs every example through lmscript.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"; cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build/host}"
HOST="$BUILD_DIR/lmfilament"
pass=0
ok(){ pass=$((pass+1)); echo "ok $pass - $1"; }
fail(){ echo "FAIL: $*" >&2; exit 1; }
[ -x "$HOST" ] || fail "$HOST not built: run scripts/build-filament.sh then scripts/build.sh"
scripts/sync-sdk.sh --verify >/dev/null || fail "SDK header pin mismatch"; ok "SDK header matches sdk.lock"
if [ "$(uname -s)" = Linux ]; then
  case "$(ldd "$HOST" 2>&1 || true)" in *"not a dynamic executable"*) ;; *) fail "$HOST is not statically linked";; esac; ok "executable is statically linked"
fi
want="lmfilament $(sed -n 's/^project(lmfilament VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt) (filament $(sed -n 's/^version=//p' filament.lock))"
[ "$("$HOST" --version)" = "$want" ] || fail "--version is '$("$HOST" --version)', want '$want'"; ok "--version prints '$want'"
LMSCRIPT="${LMSCRIPT_BIN:-lmscript}"
diff <("$LMSCRIPT" --dsl-signatures schema/lmfilament.dsl.json) <("$HOST" --dsl-signatures) >/dev/null || fail "schema/host signature drift"; ok "host signatures match the schema"
cmp <("$HOST" --dsl-schema --json) schema/lmfilament.dsl.json >/dev/null || fail "embedded schema differs"; ok "embedded schema is byte-identical"
export PATH="$PWD/$BUILD_DIR:$PATH"
for f in examples/*.lms; do
  out="$("$LMSCRIPT" "$f" 2>&1)" || { echo "$out" >&2; fail "$f"; }
  echo "$out" | grep -q "^ok " || { echo "$out" >&2; fail "$f printed no ok line"; }
  ok "$f"
done
for n in texture-modes textured-planet textured-primitives texturedquad suzanne hellotriangle hellopbr lightbulb primitives primitives-scene-graph model-viewer text caffeine-molecule ball-stick buckyball dna-double-helix cornell-box saturn-rings truss-bridge shadows multidraw cubemap bullet fluidsimulation3d fluidsimulation2d level-1-sessions level-2-orchestration level-3-hands-off-lead level-4-automation; do
  [ -s "images/$n.png" ] || fail "images/$n.png missing"
  [ "$(head -c 4 "images/$n.png" | tail -c 3)" = PNG ] || fail "images/$n.png is not a PNG"
done; ok "examples wrote PNG previews"
# A bad material must fail cleanly (exit 3, JSON error), not crash.
bad="$(mktemp -d)"; trap 'rm -rf "$bad"' EXIT
cat > "$bad/bad.lms" <<LMS
p = _dsl_ cli="lmfilament" install="github:lmctl-ai/lmfilament@latest";
let m = fil.material("bad", "material { name : bad, shadingModel : nonsense }");
end _dsl_
savedsl(p, "$bad/bad.dslb");
r = system(concat(dsl_cli(p), ["--dsl", "$bad/bad.dslb"]), 60000);
assert(r.code == 3);
println("ok bad material rejected");
LMS
"$LMSCRIPT" "$bad/bad.lms" 2>&1 | grep -q "^ok bad material rejected" || fail "bad material not rejected cleanly"; ok "bad material rejected with exit 3"
# Textures: a 2x2 nearest texture on an unlit quad shows each texel where the UVs put it (row 0 on top); misuse is a clean exit 3.
png_px() { python3 -I - "$@" <<'PY'
import sys, zlib, struct
data = open(sys.argv[1], 'rb').read(); pos = 8; idat = b''; w = h = 0
while pos < len(data):
    n, t = struct.unpack('>I4s', data[pos:pos+8]); body = data[pos+8:pos+8+n]; pos += 12 + n
    if t == b'IHDR': w, h = struct.unpack('>II', body[:8])
    elif t == b'IDAT': idat += body
raw = zlib.decompress(idat); stride = 1 + w * 3
assert all(raw[y * stride] == 0 for y in range(h)), 'unsupported PNG filter'
for xy in sys.argv[2:]:
    x, y = map(int, xy.split(',')); o = y * stride + 1 + x * 3; print(*raw[o:o+3])
PY
}
UNLIT='let m = fil.material("u", "material {\n name : u,\n shadingModel : unlit,\n requires : [ uv0 ],\n parameters : [ { type : sampler2d, name : albedo } ],\n}\nfragment {\n void material(inout MaterialInputs material) {\n prepareMaterial(material);\n material.baseColor.rgb = texture(materialParams_albedo, getUV0()).rgb;\n }\n}\n");'
run_dsl() { # NAME BODY: compile and run one snippet in $bad; prints host output, returns lmscript's exit code
  printf 'p = _dsl_ cli="lmfilament" install="github:lmctl-ai/lmfilament@latest";\n%s\nend _dsl_\nsavedsl(p, "%s.dslb");\nr = system(concat(dsl_cli(p), ["--dsl", "%s.dslb"]), 120000);\nprintln(r.stdout + r.stderr);\n' "$2" "$1" "$1" > "$bad/$1.lms"
  ( cd "$bad" && "$LMSCRIPT" "$1.lms" ) 2>&1
}
out="$(run_dsl tex1 "$UNLIT
let t = fil.texture(2, 2).filter(\"nearest\");
t.pixel(0, 0, 1.0, 0.0, 0.0); t.pixel(1, 0, 0.0, 1.0, 0.0); t.pixel(0, 1, 0.0, 0.0, 1.0); t.pixel(1, 1, 1.0, 1.0, 0.0);
m.set_texture(\"albedo\", t);
let s = fil.scene();
s.skybox(0.0, 0.0, 0.0);
s.add_part(fil.plane(2.0, 2.0), m).rotate(90.0, 0.0, 0.0);
s.camera(40.0, 0.0, 0.0, 2.6, 0.0, 0.0, 0.0);
s.render_frames(1);
s.render_png(\"tex1.png\", 100, 100);
t.save_png(\"tex1-src.png\");
let back = fil.texture_file(\"tex1-src.png\").filter(\"nearest\");
m.set_texture(\"albedo\", back);
s.add_part(fil.plane(2.0, 2.0), m).rotate(90.0, 0.0, 0.0).translate(0.0, 0.0, 0.01);
s.render_png(\"tex1b.png\", 100, 100);")"
case "$out" in *'"ok": true'*) ;; *) echo "$out"; fail "texture script";; esac
cmp "$bad/tex1.png" "$bad/tex1b.png" || fail "a texture saved with save_png and loaded with texture_file renders differently"
got="$(png_px "$bad/tex1.png" 30,30 70,30 30,70 70,70 | tr '\n' '|')"
python3 -I - "$got" <<'PY' || fail "textured quad corners (got $got)"
import sys
px = [list(map(int, p.split())) for p in sys.argv[1].strip('|').split('|')]
want = [(1,0,0),(0,1,0),(0,0,1),(1,1,0)]
for got, w in zip(px, want):
    assert all((v > 200) if c else (v < 60) for v, c in zip(got, w)), (px, w)
PY
ok "textures: 2x2 nearest quad shows red, green, blue and yellow top-left to bottom-right; PNG save/load round trip"
for case in 'fil.texture(0, 4);|between 1 and 4096' 'fil.texture(2, 2).pixel(2, 0, 0.0, 0.0, 0.0);|outside the texture' 'fil.texture(2, 2).filter("cubic");|filter must be' 'fil.texture_file("missing-file.png");|cannot open texture file' 'UNLITm.set_texture("nope", fil.texture(2, 2)); fil.scene().add(fil.plane(1.0, 1.0), m);|has no parameter' 'UNLITm.set_float("albedo", 1.0); fil.scene().add(fil.plane(1.0, 1.0), m);|is a sampler' 'UNLITfil.scene().add(fil.plane(1.0, 1.0), m).render_png("x.png", 40, 40);|' ; do
  code="${case%%|*}"; want="${case##*|}"; code="${code/UNLIT/$UNLIT }"
  out="$(run_dsl texerr "$code" || true)"
  case "$out" in *'"ok": false'*"$want"*) ;; *'"ok": true'*) [ -z "$want" ] || { echo "$out"; fail "texture error '$want' was not raised"; };; *) echo "$out"; fail "texture error '$want'";; esac
done
ok "texture misuse is a one-line error: size, pixel range, filter name, missing file, unknown or non-sampler parameter"
echo "all $pass checks passed"
