#pragma once

#include "angelscript.h"

#include <asmjit/x86.h>

#include <mutex>
#include <memory>

namespace asjitx86 {

namespace detail {
class ScalarObjectPool;
}

class X86JitCompiler : public asIJITCompiler {
public:
    explicit X86JitCompiler(asIScriptEngine* engine);
    ~X86JitCompiler() override;

    int  CompileFunction(asIScriptFunction* function, asJITFunction* output) override;
    void ReleaseJITFunction(asJITFunction func) override;
    void ClearObjectPool();

private:
    asmjit::JitRuntime m_runtime;
    std::unique_ptr<detail::ScalarObjectPool> m_objectPool;
    std::mutex         m_mutex;
};

}
