# Build, run, test

Prerequisites: cmake >= 3.24, ninja, python3, git, `lmscript` on PATH (npm `@lmctl-ai/lmctl`, DSL v5), and
**clang >= 17** (Filament requires it and C++20). Ubuntu 22.04 ships clang 14, so the project uses a user-level
LLVM:

```sh
mkdir -p ~/toolchains/llvm19 && cd ~/toolchains/llvm19
curl -LO https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/LLVM-19.1.7-Linux-X64.tar.xz
tar -xf LLVM-19.1.7-Linux-X64.tar.xz --strip-components=1
```

(The LLVM 18 ubuntu-18.04 tarball needs `libtinfo.so.5` and does not run on 22.04; 19.1.7 does.)
Both scripts honour `LLVM=/path/to/llvm`.

```sh
scripts/sync-filament.sh      # shallow checkout of the commit pinned in filament.lock into third_party/filament
scripts/build-filament.sh      # ~10 min on 4 cores; static libs in build/filament-build
scripts/build.sh               # configures + links build/host/lmfilament (static)
./test.sh                      # signatures, static link, every example, a negative test
```

Run an example with `PATH=$PWD/build/host:$PATH lmscript examples/hellopbr.lms` (materials are inlined, so most examples run from any directory;
`hellopbr` and `suzanne` read `third_party/filament/assets/models/monkey/monkey.obj`, so run those from the repo root). `examples/render-all.sh`
re-renders every README picture and copies it to `docs/images/` (`docs/videos/` for MP4).
Run a compiled script directly: `build/host/lmfilament --dsl file.dslb`.

## Filament configuration (scripts/build-filament.sh)

`-stdlib=libc++`, Release, and: samples/SDL2/tests off, **OpenGL off, Vulkan off**, XCB/XLIB off, matdbg,
fgviewer and perfetto off. Turning OpenGL off removes the `GL/gl.h` header requirement and the runtime
`dlopen` of libGL.

## Link recipe (CMakeLists.txt)

- Archives come from the Filament *build tree* (`LMFILAMENT_BUILD`), because `install` omits glslang,
  SPIRV-Tools and SPIRV-Cross. The list mirrors matc's link line, plus `filament`, `backend`, `geometry`.
- Headers come from the pinned source tree (the install prefix lacks `utils/JobSystem.h`, `filament-matp`, ...).
- Everything sits in one `--start-group ... --end-group` (circular archive references).
- `c++ c++abi pthread dl m`, then `-static`. `c++abi` must be named explicitly: clang's driver adds `-lc++`
  but not `-lc++abi`, and `operator new` plus the exception runtime live there.
- The linker warns about `getpwuid` in a static binary (`utils::Path::getUserSettingsDirectory`); it is
  never called by this host.

## Exit codes and output

One JSON line on stdout per script: `{"ok": true, "result": null}`, or an error object (`phase`,
`code`, `message`, `line`, `column`). Exit 0 ok, 2 usage, 3 load/run error. Filament's own engine
banner goes to stderr.
