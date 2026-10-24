# Gotchas

- **Toolchain.** Filament needs clang >= 17. The system clang 14 and gcc 11 cannot build it. Use
  `~/toolchains/llvm19` (see `02-build-run-test.md`).
- **Static link.** Name `c++abi` explicitly. Expect the `getpwuid` linker warning.
- **Header-only vs install.** `build/filament-install` is incomplete for embedding; use the source tree
  headers and the build-tree archives.
- **No GL/Vulkan.** Configuring with OpenGL on fails on missing `GL/gl.h` and could never work static
  (libGL is `dlopen`ed). Do not turn it on without changing the static-link goal.
- **Inside `_dsl_`** there is no `assert`, `println` or arithmetic on host results: only schema calls,
  `let`, `if`, loops and built-in math. Do checks in the outer script (exit code, `r.stdout`).
- **Material parameter errors** are reported on `Scene.add`, not on `set_float*` (the package is only
  instantiated then).
- **sRGB vs linear.** Filament light colors are linear. The examples pre-convert the upstream sRGB constants
  (0.98, 0.92, 0.89 -> 0.9551, 0.8276, 0.7678).
- **Winding.** `quad()` self-checks winding against the face normal; the sphere index order was wrong once
  (negative signed volume). If you add a primitive, verify signed volume is positive.
- **Relative paths.** Only `hellopbr` and `suzanne` still read files from `third_party/filament/...` (the monkey OBJ); run those from the repo root after `scripts/sync-filament.sh`.
- **Scene variable names.** Examples that copy loop code (buckyball, fluids, storyboards) name the scene `scn` because the source uses `s` as a loop variable.
- **Material per add.** Every `Scene.add*` builds a fresh `filament::Material` from the package, so hundreds of adds (fluid streamlines, 1000+) cost seconds; keep example part counts in the low thousands (limit 4096 per arena).
- **Engine log noise.** Filament prints its engine banner on stderr for each scene.
- **Third-party tree.** `third_party/filament` is git-ignored (54k files); `filament.lock` pins the commit.
