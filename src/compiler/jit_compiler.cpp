#include "compiler/jit_compiler.h"
#include "bytecode/helpers/object_helpers.h"
#include "codegen/emit.h"

namespace asjitx86 {

X86JitCompiler::X86JitCompiler(asIScriptEngine* engine)
    : m_objectPool(std::make_unique<detail::ScalarObjectPool>(engine)) {}
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
