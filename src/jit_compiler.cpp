#include "jit_compiler.h"

namespace asjitx86 {

X86JitCompiler::X86JitCompiler() = default;
X86JitCompiler::~X86JitCompiler() = default;

int X86JitCompiler::CompileFunction(asIScriptFunction*, asJITFunction* output) {
    if (output) *output = nullptr;
    return asERROR;
}

void X86JitCompiler::ReleaseJITFunction(asJITFunction) {}

}
