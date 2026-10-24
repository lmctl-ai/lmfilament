# Open items

1. **Real Filament pixels.** Only the CPU preview exists (`render_png`). Filament-rendered output would need a
   software GL/Vulkan path (e.g. OSMesa or SwiftShader) linked statically, which is a separate decision.
2. **Texture follow-ups.** `Material.set_texture`, `Filament.texture*` and sampling in the preview exist (see 03). Missing: KTX/KTX2 (`ktxreader` is built), cubemap and 3D samplers, texture formats other than 8-bit RGBA, shading with roughness/metallic/normal/ao maps (uploaded to Filament but unused by the preview), vertex-stage texture reads (`heightfield` displaces in the vertex shader, so it stays a stand-in), and point sprites (`point_sprites`).
3. **More upstream samples.** Not ported: skinning/morphing (`helloskinning*`, `hellomorphing`),
   `hellostereo`, `rendertarget`, `multiple_windows`, `gltf_viewer`/`gltf_instances` (gltfio is built but
   unused), `image_viewer`, `frame_generator`, benchmarks. Each needs new schema surface.
4. **Point/line primitives, vertex attributes beyond tangents/uv0/color.**
5. **IBL.** `libibl` (cmgen) is built; an `IndirectLight` + cubemap API is not exposed.
6. **Windows.** No Windows build: Filament builds with MSVC/clang-cl, and this project's static-link recipe
   (libc++, `c++abi`) is Linux/macOS-only. linux-x64/arm64 and macos-arm64/x64 are built, tested and released by CI.
7. **Packaging.** License is MIT (as lmhtml/lmcad/lmplot; compatible with Filament's Apache-2.0 and gnuplot's
   GPL-compatible terms). Releases ship `LICENSE`, `THIRD_PARTY_NOTICES.md`, `FILAMENT-LICENSE`, and on Linux
   relink objects for the statically linked glibc (LGPL). See `07-ci-and-release.md`.
