#pragma once

#include "angelscript.h"

namespace asjitx86 {

class X86JitCompiler : public asIJITCompiler {
public:
    X86JitCompiler();
    ~X86JitCompiler() override;

    int  CompileFunction(asIScriptFunction* function, asJITFunction* output) override;
    void ReleaseJITFunction(asJITFunction func) override;
};

}
