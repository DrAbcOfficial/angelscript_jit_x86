# AngelScript x86 JIT

`angelscript_jit_x86` is a 32-bit x86 JIT runtime for AngelScript 2.36.1. It compiles AngelScript bytecode to native x86 code through AsmJit while preserving the interpreter fallback and AngelScript engine lifetime rules.

## Requirements

- CMake 3.24 or newer
- A C++20 compiler
- A 32-bit x86 toolchain
- Windows or Linux

64-bit configurations are rejected during CMake configuration.

## Build

### Windows

Configure a shared build with Visual Studio:

```text
cmake -S . -B build-win32 -A Win32 -DASJITX86_BUILD_SHARED=ON
cmake --build build-win32 --config Release
ctest --test-dir build-win32 -C Release --output-on-failure
```

For a static library, use `-DASJITX86_BUILD_SHARED=OFF`.

SSE2-optimized JIT code generation is enabled by default. Disable the packed
SSE2 optimization with `-DASJITX86_ENABLE_SSE=OFF`; all builds still require
SSE2 for baseline floating-point code generation. AVX code generation is
disabled by default; enable the SSE2+AVX2 variant with
`-DASJITX86_ENABLE_AVX2=ON`.

Every build checks its required instruction set through AsmJit before creating
the JIT engine. The default build requires SSE2. The SSE2+AVX2 build additionally
requires CPU and operating system AVX2 support and does not fall back to SSE2.

### Linux

Install a multilib-capable compiler, then configure the 32-bit build:

```text
cmake -S . -B build-linux32 \
  -DCMAKE_C_FLAGS=-m32 \
  -DCMAKE_CXX_FLAGS=-m32 \
  -DASJITX86_BUILD_SHARED=ON
cmake --build build-linux32 -j
ctest --test-dir build-linux32 --output-on-failure
```

Use `-DASJITX86_BUILD_SHARED=OFF` for a static library.

To install a build:

```text
cmake --install build-linux32 --prefix /your/install/prefix
```

The install contains the public header, library files, and license documents.

## Usage

Include AngelScript and the public JIT header. Create the JIT binding after creating the AngelScript engine, and destroy the binding before releasing the engine.

```cpp
#include <angelscript.h>
#include <as_jit_x86.h>

asIScriptEngine* engine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
void* jit = AsJitCreateEngine(engine);

if (!jit) {
    engine->Release();
    return 1;
}

// Build and execute AngelScript modules normally.

AsJitDestroyEngine(jit);
engine->Release();
```

Call `AsJitGetCompatibilityError` before initialization when the host needs a
diagnostic for an unsupported CPU. It returns `nullptr` when the current CPU and
operating system satisfy the build requirements. `AsJitCreateEngine` performs
the same check and returns `nullptr` on incompatibility.

## License

The original code in this repository, excluding `third_party/`, is released under the GNU General Public License, version 3.0. See [`LICENSE`](LICENSE).

AngelScript remains under the AngelCode Scripting Library license. AsmJit remains under its original authors' license. See [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) for the license boundaries and third-party license files.
