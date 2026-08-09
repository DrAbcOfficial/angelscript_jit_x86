#pragma once

#include "angelscript.h"

namespace asjitx86 {

class X86JitCompiler;

class JitEngine {
public:
    explicit JitEngine(asIScriptEngine* engine);
    ~JitEngine();

    JitEngine(const JitEngine&) = delete;
    JitEngine& operator=(const JitEngine&) = delete;

    bool Bind();
    void Unbind();

private:
    asIScriptEngine* m_engine;
    X86JitCompiler*  m_compiler;
    bool             m_bound;
};

}
