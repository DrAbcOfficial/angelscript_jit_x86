#include "as_jit_x86.h"
#include "engine/jit_engine.h"

void* AsJitCreateEngine(asIScriptEngine* engine) {
    if (!engine) return nullptr;
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
    try {
        delete static_cast<asjitx86::JitEngine*>(jitEngine);
    } catch (...) {
    }
}
