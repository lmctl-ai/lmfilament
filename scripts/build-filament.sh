#!/usr/bin/env bash
# Build Filament static libraries (libc++, clang >= 17) (build tree only; the install prefix is not used by CMakeLists.txt).
# Usage: scripts/build-filament.sh [prefix]   (env: LLVM=/path/to/llvm, default ~/toolchains/llvm19)
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
# Compiler: $CC/$CXX if set, else $LLVM/bin/clang (default ~/toolchains/llvm19), else clang on PATH.
LLVM="${LLVM:-$HOME/toolchains/llvm19}"
if [ -z "${CXX:-}" ]; then
  if [ -x "$LLVM/bin/clang++" ]; then CC="$LLVM/bin/clang"; CXX="$LLVM/bin/clang++"; else CC=clang; CXX=clang++; fi
fi
prefix="${1:-$here/build/filament-install}"
bld="$here/build/filament-build"
export CC CXX
cmake -S "$here/third_party/filament" -B "$bld" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DCMAKE_CXX_FLAGS="-stdlib=libc++" -DCMAKE_EXE_LINKER_FLAGS="-stdlib=libc++" \
  -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON -DFILAMENT_BUILD_TESTING=OFF \
  -DFILAMENT_SUPPORTS_VULKAN=OFF -DFILAMENT_SUPPORTS_METAL=OFF -DFILAMENT_SUPPORTS_OPENGL=OFF -DFILAMENT_SUPPORTS_XCB=OFF -DFILAMENT_SUPPORTS_XLIB=OFF \
  -DFILAMENT_ENABLE_MATDBG=OFF -DFILAMENT_ENABLE_FGVIEWER=OFF -DFILAMENT_ENABLE_PERFETTO=OFF
# Only what the host links (the rest of Filament - tools, gltfio, viewer - is not needed).
cmake --build "$bld" --target filament filamat matp matlang geometry -j"$(getconf _NPROCESSORS_ONLN)"
