#include "as_jit_x86.h"
#include "angelscript.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>

namespace {

constexpr const char* kScript =
    "int main() { int sum = 0; int i = 0; int limit = 10000000; int one = 1; "
    "while (i < limit) { sum = sum + i; i = i + one; } return sum; }";

asIScriptFunction* Build(asIScriptEngine* engine, const char* name) {
    asIScriptModule* module = engine->GetModule(name, asGM_ALWAYS_CREATE);
    if (!module) return nullptr;
    if (module->AddScriptSection("benchmark", kScript) < 0) return nullptr;
    if (module->Build() < 0) return nullptr;
    return module->GetFunctionByName("main");
}

double Run(asIScriptEngine* engine, asIScriptFunction* function, asDWORD* result) {
    double best = std::numeric_limits<double>::max();
    for (int round = 0; round < 3; round++) {
        asIScriptContext* context = engine->CreateContext();
        if (!context || context->Prepare(function) < 0) return -1.0;
        auto begin = std::chrono::steady_clock::now();
        int state = context->Execute();
        auto end = std::chrono::steady_clock::now();
        if (state != asEXECUTION_FINISHED) {
            context->Release();
            return -1.0;
        }
        *result = context->GetReturnDWord();
        context->Release();
        double elapsed = std::chrono::duration<double, std::milli>(end - begin).count();
        best = std::min(best, elapsed);
    }
    return best;
}

}

int main() {
    asIScriptEngine* interpreter = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    asIScriptEngine* jitEngine = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!interpreter || !jitEngine) return 1;

    interpreter->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    jitEngine->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    void* jit = AsJitCreateEngine(jitEngine);
    if (!jit) return 1;

    asIScriptFunction* interpreterFunction = Build(interpreter, "interpreter");
    asIScriptFunction* jitFunction = Build(jitEngine, "jit");
    if (!interpreterFunction || !jitFunction) return 1;

    asDWORD interpreterResult = 0;
    asDWORD jitResult = 0;
    double interpreterMs = Run(interpreter, interpreterFunction, &interpreterResult);
    double jitMs = Run(jitEngine, jitFunction, &jitResult);
    double speedup = interpreterMs / jitMs;
    std::printf("int-loop(1e7): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterMs, jitMs, speedup);

    jitEngine->Release();
    AsJitDestroyEngine(jit);
    interpreter->Release();

    return interpreterMs > 0.0 && jitMs > 0.0 && interpreterResult == jitResult && speedup >= 1.0
        ? 0
        : 1;
}
