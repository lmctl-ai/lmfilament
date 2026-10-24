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

Releases are automatic on every push to `main` (see "Automatic releases" below); a manual `vX.Y.Z` tag push still works.

Status: the Linux x64 path is what was developed and run locally. Linux arm64 and macOS use the same scripts
but are only verified by CI runs; check the Actions tab before trusting those assets.

## Automatic releases

Every push to `main` that CI accepts is released automatically. The `version` job in `.github/workflows/ci.yml` picks `MAJOR.MINOR.<run number>` (MAJOR.MINOR come from the version source in the repo, the patch is the workflow run number), every build job runs `scripts/stamp-version.sh` to write that version into the working tree before building (never committed), so the binary reports it. When all platform builds and tests pass, the `release` job creates the tag `vMAJOR.MINOR.N` on the pushed commit and publishes the assets, marked latest. Pull requests and manual runs never release. Pushing a `v*` tag by hand still releases exactly that version. To move to a new MAJOR.MINOR, change the version source (`project(... VERSION x.y.z)` in `CMakeLists.txt`, or the `VERSION` file for lmblender) in a commit.
