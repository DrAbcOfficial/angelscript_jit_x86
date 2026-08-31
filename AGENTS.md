# angelscript_jit_x86

32-bit x86 JIT for **AngelScript 2.36.1**. Public API is only `include/as_jit_x86.h`: `AsJitCreateEngine`, `AsJitDestroyEngine`, `AsJitGetCompatibilityError`. Internals include AngelScript **private** headers (`as_scriptengine.h`, `as_scriptfunction.h`, `as_context.h`) — layout-sensitive.

## Layout

- `src/api` — C ABI + CPU checks.
- `src/engine` — `SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS)` + `SetJITCompiler`.
- `src/compiler` — `asIJITCompiler`; mutex around compile/release.
- `src/codegen/emit` — native emit (`control_flow`, `stack`, `calls`, `references`, `memory`, `numeric`).
- `src/bytecode` — C helpers for opcodes without a native path (`GetJitBcHelper`). Unhandled emit falls through to helpers; missing helper is a hard compile failure.
- `third_party/angelscript` — **vendored** SDK 2.36.1 (not a submodule).
- `third_party/asmjit` — fork submodule (`DrAbcOfficial/asmjit`), linked static.

Do not bump AngelScript without re-checking private types and `asSVMRegisters`.

## Build

CMake 3.24+, C++20, 32-bit only. Windows: `-A Win32`. Linux: `-DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32` at configure time.

```text
cmake -S . -B build-win32 -A Win32 -DASJITX86_BUILD_SHARED=ON
cmake --build build-win32 --config Release
ctest --test-dir build-win32 -C Release --output-on-failure
```

- `ASJITX86_BUILD_SHARED` default ON; sven_jit forces OFF.
- `ASJITX86_ENABLE_SSE` default ON (packed float groups). Still requires SSE2 even if OFF.
- `ASJITX86_ENABLE_AVX2` default OFF; requires SSE; no runtime fallback to SSE2.
- Host C++ is `/arch:IA32` / `-mno-sse`; SIMD only in generated code.
- `.gitignore` covers `build/` and `build-static/` only — do not commit `build-win32/` (untracked).

Create the JIT after the engine; destroy the JIT before `engine->Release()`. `JitEngine` AddRefs the engine.

## Tests

`ctest` runs `jit_consistency` (cwd `tests/`) and `cpu_requirements`. Benchmark/showcase binaries are not tests.

Consistency: each `tests/scripts/*.as` listed in `tests/consistency/jit_consistency.cpp` is built and run on interpreter vs JIT (`main` return, `g_out`, exception). Adding a script means adding the file **and** the array entry. Optional argv: script directory (default `scripts`).

```text
ctest --test-dir build-win32 -C Release -R jit_consistency --output-on-failure
```

AVX2: extra configure with `-DASJITX86_ENABLE_AVX2=ON` in a separate build dir.

Packed SSE/AVX float emit is skipped when `cacheLocals_` is on (double arithmetic + `variableSpace <= 64` + cacheable ops).
