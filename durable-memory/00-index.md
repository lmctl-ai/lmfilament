# lmfilament: start here

lmfilament is a static, dependency-free lmscript `_dsl_` host that embeds Google Filament 1.78.1 (noop backend: materials,
engine objects, geometry) and draws pictures with its own CPU preview rasterizer. Repo: https://github.com/lmctl-ai/lmfilament.

## 60-second takeover

```sh
scripts/sync-filament.sh          # third_party/filament at filament.lock (git-ignored)
scripts/build-filament.sh         # static Filament libs, needs clang >= 17 (~/toolchains/llvm19), ~10 min
scripts/build.sh                  # build/host/lmfilament, fully static
PATH=$PWD/build/host:$PATH ./test.sh   # signatures, static link, every example, negative test
```

Change flow: edit `schema/lmfilament.dsl.json` + `register_host()` in `cpp/main.cpp` (and `cpp/preview.cpp` if pictures change), rebuild,
run `./test.sh`, update docs, `git commit && git push origin main` (CI releases automatically).

## Doc map

1. `01-purpose-and-place.md`: what is real (materials, engine, geometry) and what is not (Filament pixels).
2. `02-build-run-test.md`: toolchain, build scripts, link recipe, exit codes.
3. `03-architecture-and-schema.md`: source layout, object model, scene graph, preview renderer, generated schema surface.
4. `04-gotchas.md`: things that already bit once.
5. `05-open-items.md`: what is not done and why.
6. `06-upstream-sample-map.md`: which Filament / Magnum / lmblender / lmcad scene each example ports and what it drops.
7. `07-ci-and-release.md`: CI matrix, automatic releases.
20. `20-sibling-3d-hosts-and-conventions.md`: the four 3D repos side by side, DSL cheat sheet, porting rules, README conventions, **mandatory working conventions**.

## State (2026-10-24)

Built and released on linux-x64/arm64, macos-arm64/x64. 35 examples, all pass `./test.sh`. Open: real Filament pixels, textures, IBL, more
upstream samples, Windows (see 05).
