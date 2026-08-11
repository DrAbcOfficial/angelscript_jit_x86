#include "compiler/jit_compiler.h"
#include "codegen/emit.h"

namespace asjitx86 {

namespace {

bool IsTinyDestructor(asIScriptFunction* function) {
    const char* name = function->GetName();
    if (!function->GetObjectType() || !name || name[0] != '~') return false;

    asUINT length = 0;
    asDWORD* bytecode = function->GetByteCode(&length);
    if (!bytecode) return false;
    unsigned operations = 0;
    for (asUINT offset = 0; offset < length;) {
        asEBCInstr op = static_cast<asEBCInstr>(bytecode[offset] & 0xFF);
        if (op != asBC_JitEntry && ++operations > 4) return false;
        offset += asBCTypeSize[asBCInfo[op].type];
    }
    return true;
}

}

X86JitCompiler::X86JitCompiler() = default;
X86JitCompiler::~X86JitCompiler() = default;

int X86JitCompiler::CompileFunction(asIScriptFunction* function, asJITFunction* output) {
    if (!function || !output) return asERROR;
    *output = nullptr;
    if (IsTinyDestructor(function)) return asSUCCESS;
    std::lock_guard<std::mutex> lock(m_mutex);
    return EmitFunction(m_runtime, function, output);
}

void X86JitCompiler::ReleaseJITFunction(asJITFunction func) {
    if (!func) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_runtime.release(reinterpret_cast<void*>(func));
}

}
