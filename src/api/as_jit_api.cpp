#include "as_jit_x86.h"
#include "api/cpu_requirements.h"
#include "engine/jit_engine.h"

#include <asmjit/core.h>

const char* AsJitGetCompatibilityError(void) {
    return asjitx86::detail::CheckCpuCompatibility(
        asmjit::CpuInfo::host().features());
}

void* AsJitCreateEngine(asIScriptEngine* engine) {
    if (!engine || AsJitGetCompatibilityError()) return nullptr;
    try {
        auto* jit = new asjitx86::JitEngine(engine);
        if (!jit->Bind()) {
            delete jit;
            return nullptr;
        }
        return static_cast<void*>(jit);
    } catch (...) {
        return nullptr;
    }
}

void AsJitDestroyEngine(void* jitEngine) {
    if (!jitEngine) return;
    delete static_cast<asjitx86::JitEngine*>(jitEngine);
}
