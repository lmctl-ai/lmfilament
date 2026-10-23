# CI and release

Workflow: `.github/workflows/ci.yml` (actions pinned by SHA, as in lmhtml).

- Matrix: linux-x64 (ubuntu-24.04), linux-arm64 (ubuntu-24.04-arm), macos-arm64 (macos-15), macos-x64 (macos-15-intel).
  `workflow_dispatch` takes an `only` input to run one job. Docs-only pushes skip CI.
- Each job: toolchain (Linux: clang-18 + libc++/libc++abi/libunwind/compiler-rt dev packages; macOS: Apple clang),
  `scripts/sync-filament.sh` (shallow fetch of the commit in `filament.lock`), `actions/cache` of
  `build/filament-build` keyed on `filament.lock` + `scripts/build-filament.sh`, npm `@lmctl-ai/lmctl@0.2.401`,
  `build-filament.sh`, `build.sh`, `test.sh`, then package + upload artifact.
- Release: pushing a tag `vX.Y.Z` runs the `release` job after all build jobs pass. It checks that the tag equals
  `lmfilament --version` (bump `project(lmfilament VERSION ...)` in CMakeLists.txt first), then creates or updates the
  GitHub release with `lmfilament-<platform>` binaries, `.sha256` files, Linux relink-object archives, and notices.

To cut a release: bump VERSION, commit, `git tag vX.Y.Z && git push origin vX.Y.Z`.

Status: the Linux x64 path is what was developed and run locally. Linux arm64 and macOS use the same scripts
but are only verified by CI runs; check the Actions tab before trusting those assets.
