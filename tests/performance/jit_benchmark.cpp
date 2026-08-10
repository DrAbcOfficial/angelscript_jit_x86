#include "as_jit_x86.h"
#include "angelscript.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>

namespace {

constexpr const char* kIntegerScript =
    "int main() { int sum = 0; int i = 0; int limit = 10000000; int one = 1; "
    "while (i < limit) { sum = sum + i; i = i + one; } return sum; }";

constexpr const char* kSystemCallScript =
    "int main() { int value = 0; int i = 0; int limit = 1000000; "
    "while (i < limit) { value = addOne(value); i++; } return value; }";

constexpr const char* kFunctionCallScript =
    "funcdef int Unary(int); int main() { Unary@ fn = @addOne; "
    "int value = 0; int i = 0; int limit = 1000000; "
    "while (i < limit) { value = fn(value); i++; } return value; }";

constexpr const char* kImportedCallScript =
    "import int hostAdd(int) from 'host'; int main() { "
    "int value = 0; int i = 0; int limit = 1000000; "
    "while (i < limit) { value = hostAdd(value); i++; } return value; }";

int AddOne(int value) {
    return value + 1;
}

asIScriptFunction* Build(asIScriptEngine* engine, const char* name, const char* script) {
    asIScriptModule* module = engine->GetModule(name, asGM_ALWAYS_CREATE);
    if (!module) return nullptr;
    if (module->AddScriptSection("benchmark", script) < 0) return nullptr;
    if (module->Build() < 0) return nullptr;
    return module->GetFunctionByName("main");
}

asIScriptFunction* BuildImported(asIScriptEngine* engine, const char* name) {
    asIScriptFunction* function = Build(engine, name, kImportedCallScript);
    if (!function) return nullptr;
    asIScriptModule* module = function->GetModule();
    asIScriptFunction* imported = engine->GetGlobalFunctionByDecl("int addOne(int)");
    if (!module || !imported || module->BindImportedFunction(0, imported) < 0) return nullptr;
    return function;
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

    if (interpreter->RegisterGlobalFunction("int addOne(int)", asFUNCTION(AddOne), asCALL_CDECL) < 0)
        return 1;
    if (jitEngine->RegisterGlobalFunction("int addOne(int)", asFUNCTION(AddOne), asCALL_CDECL) < 0)
        return 1;

    asIScriptFunction* interpreterFunction = Build(interpreter, "interpreter", kIntegerScript);
    asIScriptFunction* jitFunction = Build(jitEngine, "jit", kIntegerScript);
    if (!interpreterFunction || !jitFunction) return 1;

    asDWORD interpreterResult = 0;
    asDWORD jitResult = 0;
    double interpreterMs = Run(interpreter, interpreterFunction, &interpreterResult);
    double jitMs = Run(jitEngine, jitFunction, &jitResult);
    double speedup = interpreterMs / jitMs;
    std::printf("int-loop(1e7): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterMs, jitMs, speedup);

    asIScriptFunction* interpreterSystemFunction =
        Build(interpreter, "interpreter-system", kSystemCallScript);
    asIScriptFunction* jitSystemFunction = Build(jitEngine, "jit-system", kSystemCallScript);
    if (!interpreterSystemFunction || !jitSystemFunction) return 1;

    asDWORD interpreterSystemResult = 0;
    asDWORD jitSystemResult = 0;
    double interpreterSystemMs = Run(interpreter, interpreterSystemFunction, &interpreterSystemResult);
    double jitSystemMs = Run(jitEngine, jitSystemFunction, &jitSystemResult);
    double systemSpeedup = interpreterSystemMs / jitSystemMs;
    std::printf("system-call-loop(1e6): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterSystemMs, jitSystemMs, systemSpeedup);

    asIScriptFunction* interpreterFunctionCall =
        Build(interpreter, "interpreter-function", kFunctionCallScript);
    asIScriptFunction* jitFunctionCall = Build(jitEngine, "jit-function", kFunctionCallScript);
    if (!interpreterFunctionCall || !jitFunctionCall) return 1;

    asDWORD interpreterFunctionResult = 0;
    asDWORD jitFunctionResult = 0;
    double interpreterFunctionMs = Run(interpreter, interpreterFunctionCall, &interpreterFunctionResult);
    double jitFunctionMs = Run(jitEngine, jitFunctionCall, &jitFunctionResult);
    double functionSpeedup = interpreterFunctionMs / jitFunctionMs;
    std::printf("function-call-loop(1e6): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterFunctionMs, jitFunctionMs, functionSpeedup);

    asIScriptFunction* interpreterImportedCall = BuildImported(interpreter, "interpreter-imported");
    asIScriptFunction* jitImportedCall = BuildImported(jitEngine, "jit-imported");
    if (!interpreterImportedCall || !jitImportedCall) return 1;

    asDWORD interpreterImportedResult = 0;
    asDWORD jitImportedResult = 0;
    double interpreterImportedMs = Run(interpreter, interpreterImportedCall, &interpreterImportedResult);
    double jitImportedMs = Run(jitEngine, jitImportedCall, &jitImportedResult);
    double importedSpeedup = interpreterImportedMs / jitImportedMs;
    std::printf("imported-call-loop(1e6): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterImportedMs, jitImportedMs, importedSpeedup);

    jitEngine->Release();
    AsJitDestroyEngine(jit);
    interpreter->Release();

    return interpreterMs > 0.0 && jitMs > 0.0 &&
                   interpreterSystemMs > 0.0 && jitSystemMs > 0.0 &&
                   interpreterFunctionMs > 0.0 && jitFunctionMs > 0.0 &&
                   interpreterImportedMs > 0.0 && jitImportedMs > 0.0 &&
                   interpreterResult == jitResult && interpreterSystemResult == jitSystemResult &&
                   interpreterFunctionResult == jitFunctionResult &&
                   interpreterImportedResult == jitImportedResult &&
                   speedup >= 1.0
        ? 0
        : 1;
}
