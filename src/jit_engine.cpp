#include "jit_engine.h"
#include "jit_compiler.h"

namespace asjitx86 {

JitEngine::JitEngine(asIScriptEngine* engine)
    : m_engine(engine), m_compiler(new X86JitCompiler()), m_bound(false) {}

JitEngine::~JitEngine() {
    Unbind();
    delete m_compiler;
}

bool JitEngine::Bind() {
    if (m_bound) return true;
    int r = m_engine->SetJITCompiler(m_compiler);
    if (r < 0) return false;
    m_bound = true;
    return true;
}

void JitEngine::Unbind() {
    if (!m_bound) return;
    m_engine->SetJITCompiler(nullptr);
    m_bound = false;
}

}
