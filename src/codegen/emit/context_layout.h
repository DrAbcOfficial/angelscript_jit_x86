#pragma once

// Numeric offsets into engine-private structures used by the native
// call/return sequences emitted in calls.cpp. They mirror what the C++
// helpers in runtime_helpers.cpp do with the same members; keep both in
// sync when the vendored AngelScript is updated.

#include "angelscript.h"

#include "as_context.h"
#include "as_scriptfunction.h"

#include <cstddef>
#include <cstdint>

namespace asjitx86::emit {

// asCArray<T> is { T* array; asUINT length; asUINT maxLength; char buf[] }.
inline constexpr uint32_t kCtxStatus = offsetof(asCContext, m_status);
inline constexpr uint32_t kCtxCurrentFunction =
    offsetof(asCContext, m_currentFunction);
inline constexpr uint32_t kCtxCallStack = offsetof(asCContext, m_callStack);
inline constexpr uint32_t kCtxStackBlocks = offsetof(asCContext, m_stackBlocks);
inline constexpr uint32_t kCtxStackIndex = offsetof(asCContext, m_stackIndex);

inline constexpr uint32_t kArrayLength = 4;
inline constexpr uint32_t kArrayMaxLength = 8;

// asCArray keeps its data members protected, so offsetof() cannot reach
// them on MSVC. Mirror the layout instead and pin the total size; the
// JIT compiler double-checks the field offsets once at runtime.
// buf is 2*4*AS_PTR_SIZE bytes and AS_PTR_SIZE is 1 on 32-bit targets.
struct AsArrayMirror {
    void* array;
    uint32_t length;
    uint32_t maxLength;
    char buf[8];
};
static_assert(sizeof(AsArrayMirror) == sizeof(asCArray<size_t>),
              "asCArray<size_t> layout changed; update kArray* offsets");
static_assert(sizeof(AsArrayMirror) == sizeof(asCArray<asDWORD>),
              "asCArray<asDWORD> layout changed; update kArray* offsets");

inline constexpr uint32_t kFnScriptData =
    offsetof(asCScriptFunction, scriptData);
inline constexpr uint32_t kDataJitFunction =
    offsetof(asCScriptFunction::ScriptFunctionData, jitFunction);
inline constexpr uint32_t kDataByteCode =
    offsetof(asCScriptFunction::ScriptFunctionData, byteCode);
inline constexpr uint32_t kDataVariableSpace =
    offsetof(asCScriptFunction::ScriptFunctionData, variableSpace);
inline constexpr uint32_t kDataStackNeeded =
    offsetof(asCScriptFunction::ScriptFunctionData, stackNeeded);

// Calls into asCScriptFunction/ScriptFunctionData rely on these members
// staying at the same offsets the C++ helpers see.
static_assert(offsetof(asCScriptFunction::ScriptFunctionData, jitFunction) ==
              kDataJitFunction);
static_assert(offsetof(asCScriptFunction::ScriptFunctionData, byteCode) ==
              kDataByteCode);
static_assert(offsetof(asCScriptFunction::ScriptFunctionData, variableSpace) ==
              kDataVariableSpace);
static_assert(offsetof(asCScriptFunction::ScriptFunctionData, stackNeeded) ==
              kDataStackNeeded);

}  // namespace asjitx86::emit
