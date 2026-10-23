#!/usr/bin/env bash
# Fetch Filament at the commit pinned in filament.lock into third_party/filament (shallow).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
repo="$(sed -n 's/^repo=//p' "$here/filament.lock")"
commit="$(sed -n 's/^commit=//p' "$here/filament.lock")"
dir="$here/third_party/filament"
if [ -d "$dir/.git" ] && [ "$(git -C "$dir" rev-parse HEAD)" = "$commit" ]; then echo "filament already at $commit"; exit 0; fi
mkdir -p "$dir"; cd "$dir"
[ -d .git ] || git init -q
git fetch -q --depth 1 "$repo" "$commit"
git checkout -q --force FETCH_HEAD
echo "filament at $(git rev-parse HEAD)"
