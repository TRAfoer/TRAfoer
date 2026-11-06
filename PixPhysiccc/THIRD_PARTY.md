# Third-party source dependencies

`python tools/bootstrap.py` fetches the following exact source revisions into the
ignored `vendor/` directory. GNU Make compiles their source directly; CMake is
not needed. Archives come from fixed-commit codeload URLs and are checked against
pinned SHA-256 hashes before extraction. Only source trees and root documents and
licenses are extracted. Per-file checksums verify existing archive installations;
existing exact Git checkouts are accepted after revision and modification checks.

| Dependency | Release | Exact commit | License |
| --- | --- | --- | --- |
| [raylib](https://github.com/raysan5/raylib) | 5.5 | `c1ab645ca298a2801097931d1079b10ff7eb9df8` | zlib/libpng, `vendor/raylib/LICENSE` |
| [Jolt Physics](https://github.com/jrouwe/JoltPhysics) | v5.3.0 | `0373ec0dd762e4bc2f6acdb08371ee84fa23c6db` | MIT, `vendor/JoltPhysics/LICENSE` |

All upstream license and attribution files remain in their checkouts. raylib
also includes bundled libraries under `vendor/raylib/src/external/`, with their
license notices embedded in those sources. The build uses raylib's bundled GLFW,
image, font, model and audio support rather than fetching separate dependencies.

Pinned archive SHA-256 hashes:

- raylib: `be3443c3fb97f7cbd18ae3f0ea2a73e6c1486128df33b639dce7e44070fc285d`
- Jolt Physics: `834644d7e9dffee9be0220748015667649d3361611b01c7e6839a5eb58e89711`

Jolt is a static, single-precision, C++17 build with its default allocator and
object-layer size. Object streams, debug rendering, profiling, RTTI and C++
exceptions are disabled; the same configuration is used by `src/rigid.cpp`.
No AVX or newer SIMD instruction set is required beyond the compiler's default
x86-64 baseline. The Windows executables link GCC's C/C++ runtimes statically.
