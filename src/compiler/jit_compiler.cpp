#include "compiler/jit_compiler.h"
#include "bytecode/helpers/object_helpers.h"
#include "codegen/emit.h"
#include "codegen/emit/context_layout.h"

#include "as_context.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace asjitx86 {

namespace {

// Secondary runtime check for the asCArray offsets hardcoded in
// context_layout.h (protected members cannot be offsetof'd on MSVC).
void VerifyEngineLayoutAssumptions(asIScriptEngine* engine) {
    asCArray<size_t> probe(3);
    probe.SetLengthNoConstruct(2);
    assert(reinterpret_cast<uint32_t*>(&probe)[1] == 2);
    assert(reinterpret_cast<uint32_t*>(&probe)[2] == 3);

    (void)engine;
}

}  // namespace

X86JitCompiler::X86JitCompiler(asIScriptEngine* engine)
    : m_objectPool(std::make_unique<detail::ScalarObjectPool>(engine)) {
    VerifyEngineLayoutAssumptions(engine);
}
X86JitCompiler::~X86JitCompiler() = default;

int X86JitCompiler::CompileFunction(asIScriptFunction* function, asJITFunction* output) {
    if (!function || !output) return asERROR;
    *output = nullptr;
    std::lock_guard<std::mutex> lock(m_mutex);
    return EmitFunction(m_runtime, *m_objectPool, function, output);
}

void X86JitCompiler::ClearObjectPool() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_objectPool->Clear();
}

void X86JitCompiler::ReleaseJITFunction(asJITFunction func) {
    if (!func) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_runtime.release(reinterpret_cast<void*>(func));
}

}
