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

constexpr const char* kLocalCopyScript =
    "int main() { int source = 1; int destination = 0; int i = 0; int limit = 10000000; "
    "while (i < limit) { destination = source; source = destination + 1; i++; } "
    "return destination; }";

constexpr const char* kLocalCopy64Script =
    "int main() { int64 source = 1; int64 destination = 0; int i = 0; int limit = 10000000; "
    "while (i < limit) { destination = source; source = destination + 1; i++; } "
    "return int(destination); }";

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

bool HasOpcode(asIScriptFunction* function, asEBCInstr expected) {
    asUINT length = 0;
    asDWORD* bytecode = function->GetByteCode(&length);
    if (!bytecode) return false;
    asDWORD* current = bytecode;
    asDWORD* end = bytecode + length;
    while (current < end) {
        asEBCInstr opcode = static_cast<asEBCInstr>(*current & 0xFF);
        if (opcode == expected) return true;
        current += asBCTypeSize[asBCInfo[opcode].type];
    }
    return false;
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
    if (!interpreterSystemFunction || !jitSystemFunction ||
        !HasOpcode(jitSystemFunction, asBC_CpyVtoR4) ||
        !HasOpcode(jitSystemFunction, asBC_CpyRtoV4))
        return 1;

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

    asIScriptFunction* interpreterLocalCopy = Build(interpreter, "interpreter-local-copy", kLocalCopyScript);
    asIScriptFunction* jitLocalCopy = Build(jitEngine, "jit-local-copy", kLocalCopyScript);
    if (!interpreterLocalCopy || !jitLocalCopy ||
        !HasOpcode(jitLocalCopy, asBC_SetV4) || !HasOpcode(jitLocalCopy, asBC_CpyVtoV4))
        return 1;

    asDWORD interpreterLocalCopyResult = 0;
    asDWORD jitLocalCopyResult = 0;
    double interpreterLocalCopyMs = Run(interpreter, interpreterLocalCopy, &interpreterLocalCopyResult);
    double jitLocalCopyMs = Run(jitEngine, jitLocalCopy, &jitLocalCopyResult);
    double localCopySpeedup = interpreterLocalCopyMs / jitLocalCopyMs;
    std::printf("local-copy-loop(1e7): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterLocalCopyMs, jitLocalCopyMs, localCopySpeedup);

    asIScriptFunction* interpreterLocalCopy64 =
        Build(interpreter, "interpreter-local-copy64", kLocalCopy64Script);
    asIScriptFunction* jitLocalCopy64 = Build(jitEngine, "jit-local-copy64", kLocalCopy64Script);
    if (!interpreterLocalCopy64 || !jitLocalCopy64 ||
        !HasOpcode(jitLocalCopy64, asBC_SetV8) ||
        !HasOpcode(jitLocalCopy64, asBC_CpyVtoV8) ||
        !HasOpcode(jitLocalCopy64, asBC_ADDi64))
        return 1;

    asDWORD interpreterLocalCopy64Result = 0;
    asDWORD jitLocalCopy64Result = 0;
    double interpreterLocalCopy64Ms =
        Run(interpreter, interpreterLocalCopy64, &interpreterLocalCopy64Result);
    double jitLocalCopy64Ms = Run(jitEngine, jitLocalCopy64, &jitLocalCopy64Result);
    double localCopy64Speedup = interpreterLocalCopy64Ms / jitLocalCopy64Ms;
    std::printf("local-copy64-loop(1e7): interpreter=%.3f ms jit=%.3f ms speedup=%.2fx\n",
                interpreterLocalCopy64Ms, jitLocalCopy64Ms, localCopy64Speedup);

    jitEngine->Release();
    AsJitDestroyEngine(jit);
    interpreter->Release();

    return interpreterMs > 0.0 && jitMs > 0.0 &&
                   interpreterSystemMs > 0.0 && jitSystemMs > 0.0 &&
                   interpreterFunctionMs > 0.0 && jitFunctionMs > 0.0 &&
                   interpreterImportedMs > 0.0 && jitImportedMs > 0.0 &&
                   interpreterLocalCopyMs > 0.0 && jitLocalCopyMs > 0.0 &&
                   interpreterLocalCopy64Ms > 0.0 && jitLocalCopy64Ms > 0.0 &&
                   interpreterResult == jitResult && interpreterSystemResult == jitSystemResult &&
                   interpreterFunctionResult == jitFunctionResult &&
                   interpreterImportedResult == jitImportedResult &&
                   interpreterLocalCopyResult == jitLocalCopyResult &&
                   interpreterLocalCopy64Result == jitLocalCopy64Result &&
                   speedup >= 1.0 && localCopy64Speedup >= 1.0
        ? 0
        : 1;
}
