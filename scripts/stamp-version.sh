#!/usr/bin/env bash
# Stamp a release version into the working tree (used by CI only; the change is never committed).
#   scripts/stamp-version.sh --base    print the current MAJOR.MINOR
#   scripts/stamp-version.sh X.Y.Z     rewrite the version source to X.Y.Z (no-op when empty)
set -euo pipefail
cd "$(dirname "$0")/.."
if [ -f VERSION ]; then
  current="$(head -n1 VERSION)"
else
  current="$(sed -n 's/^project([A-Za-z0-9_]* VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -n1)"
fi
[ -n "$current" ] || { echo "cannot find the version source" >&2; exit 1; }
if [ "${1:-}" = "--base" ]; then echo "${current%.*}"; exit 0; fi
v="${1:-}"
[ -n "$v" ] || exit 0
if [ -f VERSION ]; then
  printf '%s\n' "$v" > VERSION
else
  sed -i.bak "s/^\(project([A-Za-z0-9_]* VERSION \)[0-9][0-9.]*/\1$v/" CMakeLists.txt && rm -f CMakeLists.txt.bak
fi
echo "stamped version $v (was $current)"
