#!/usr/bin/env bash
# Regenerate every README picture: runs each example (writes images/NAME.png) and copies the results to docs/images/.
# Usage (from anywhere, with build/host built): examples/render-all.sh
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="$PWD/build/host:$PATH"
mkdir -p images docs/images
for f in examples/*.lms; do lmscript "$f" | grep -q '^ok ' || { echo "FAILED: $f" >&2; exit 1; }; done
cp images/*.png docs/images/
cp images/*.gif docs/images/ 2>/dev/null || true
mkdir -p docs/videos
cp images/*.mp4 docs/videos/ 2>/dev/null || true
ls docs/images
