#pragma once

#include "angelscript.h"

#include <asmjit/x86.h>

#include <mutex>

namespace asjitx86 {

class X86JitCompiler : public asIJITCompiler {
public:
    X86JitCompiler();
    ~X86JitCompiler() override;

    int  CompileFunction(asIScriptFunction* function, asJITFunction* output) override;
    void ReleaseJITFunction(asJITFunction func) override;

private:
    asmjit::JitRuntime m_runtime;
    std::mutex         m_mutex;
};

}
