#include "engine/jit_engine.h"
#include "compiler/jit_compiler.h"

namespace asjitx86 {

JitEngine::JitEngine(asIScriptEngine* engine)
    : m_engine(engine), m_compiler(new X86JitCompiler(engine)) {
    m_engine->AddRef();
}

JitEngine::~JitEngine() {
    m_compiler->ClearObjectPool();
    m_engine->Release();
    delete m_compiler;
}

bool JitEngine::Bind() {
    m_engine->SetEngineProperty(asEP_INCLUDE_JIT_INSTRUCTIONS, true);
    return m_engine->SetJITCompiler(m_compiler) >= 0;
}

}
