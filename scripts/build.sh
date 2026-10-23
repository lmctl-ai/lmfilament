#!/usr/bin/env bash
# Configure and build the lmfilament executable (run scripts/build-filament.sh first).
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
LLVM="${LLVM:-$HOME/toolchains/llvm19}"
if [ -z "${CXX:-}" ]; then
  if [ -x "$LLVM/bin/clang++" ]; then CC="$LLVM/bin/clang"; CXX="$LLVM/bin/clang++"; else CC=clang; CXX=clang++; fi
fi
export CC CXX
ldflags="-stdlib=libc++"
[ "$(uname -s)" = Linux ] && ldflags="$ldflags -rtlib=compiler-rt -unwindlib=libunwind"
cmake -S "$here" -B "$here/build/host" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_EXE_LINKER_FLAGS="$ldflags"
cmake --build "$here/build/host"
