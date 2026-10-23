# Open items

1. **Real Filament pixels.** Only the CPU preview exists (`render_png`). Filament-rendered output would need a
   software GL/Vulkan path (e.g. OSMesa or SwiftShader) linked statically, which is a separate decision.
2. **Textures.** `Material.set_texture`, KTX/PNG loading and sampler parameters would unlock the textured
   parts of `suzanne`, `texturedquad`, `heightfield`, `point_sprites`. The `libimage`/`ktxreader`/`stb`
   archives are already built.
3. **More upstream samples.** Not ported: skinning/morphing (`helloskinning*`, `hellomorphing`),
   `hellostereo`, `rendertarget`, `multiple_windows`, `gltf_viewer`/`gltf_instances` (gltfio is built but
   unused), `image_viewer`, `frame_generator`, benchmarks. Each needs new schema surface.
4. **Point/line primitives, vertex attributes beyond tangents/uv0/color.**
5. **IBL.** `libibl` (cmgen) is built; an `IndirectLight` + cubemap API is not exposed.
6. **Windows.** No Windows build: Filament builds with MSVC/clang-cl, and this project's static-link recipe
   (libc++, `c++abi`) is Linux/macOS-only. macOS (arm64, x64) is in CI but unverified locally.
7. **Packaging.** License is MIT (as lmhtml/lmcad/lmplot; compatible with Filament's Apache-2.0 and gnuplot's
   GPL-compatible terms). Releases ship `LICENSE`, `THIRD_PARTY_NOTICES.md`, `FILAMENT-LICENSE`, and on Linux
   relink objects for the statically linked glibc (LGPL). See `07-ci-and-release.md`.
