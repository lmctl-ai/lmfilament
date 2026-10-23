Third-party components in the lmfilament distribution
=====================================================

Project license
---------------
lmfilament-authored source is licensed under the MIT License; see `LICENSE`. This includes the vendored
`cpp/dslvm.hpp`, which originates from the MIT-licensed cpp-dsl-sdk. MIT is compatible with every component
below, and with gnuplot's GPL-compatible terms as used by the sibling lmplot project.

Google Filament 1.78.1 (pinned by `filament.lock`)
--------------------------------------------------
The executable statically links the Filament engine, filamat (material compiler), filament-matp, geometry,
filabridge, filaflat, math and utils libraries. Filament is copyright Google LLC and licensed under the
Apache License 2.0; the exact text shipped with the build is `FILAMENT-LICENSE`.
https://github.com/google/filament

Libraries built from Filament's `third_party/` and linked in
------------------------------------------------------------
Each keeps its own license in the pinned Filament checkout (`third_party/<name>/`):

- glslang (BSD-3-Clause, with Apache-2.0 and MIT parts) - GLSL -> SPIR-V
- SPIRV-Tools (Apache-2.0) - SPIR-V optimizer
- SPIRV-Cross (Apache-2.0 / MIT) - SPIR-V -> GLSL/MSL
- zstd (BSD-3-Clause; used under BSD) - material compression
- smol-v (MIT / public domain) - SPIR-V compression
- meshoptimizer (MIT)
- mikktspace (zlib)
- robin-map (MIT) - headers

LLVM libc++, libc++abi, libunwind, compiler-rt
----------------------------------------------
Licensed Apache-2.0 with LLVM exceptions; statically linked on Linux (system libc++ on macOS).

GNU C Library (Linux builds)
----------------------------
The fully static Linux binary includes glibc static objects (libc, libm, libpthread, libdl). glibc is
LGPL-2.1-or-later. Each Linux release includes `lmfilament-linux-<arch>-relink-objects.tar.gz` (the
application object files and the ninja link line) so the program can be relinked against a modified glibc.
The binary also pulls in `getpwuid` through Filament's `utils::Path`; that code path is not used by lmfilament.
