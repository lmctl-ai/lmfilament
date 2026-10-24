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
for n in hellotriangle hellopbr lightbulb primitives primitives-scene-graph model-viewer text; do
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
echo "all $pass checks passed"
