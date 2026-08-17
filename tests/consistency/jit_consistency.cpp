#include "as_jit_x86.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptbuilder.h"
#include "scriptdictionary.h"
#include "scriptstdstring.h"
#include "as_context.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static int s_checks = 0;
static int s_failures = 0;

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        s_checks++;                                                             \
        auto va_ = (a);                                                         \
        auto vb_ = (b);                                                         \
        if (!(va_ == vb_)) {                                                    \
            s_failures++;                                                       \
            std::printf("FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b);  \
        }                                                                       \
    } while (0)

#define CHECK_TRUE(expr)                                                        \
    do {                                                                        \
        s_checks++;                                                             \
        if (!(expr)) {                                                          \
            s_failures++;                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);         \
        }                                                                       \
    } while (0)

namespace {

void MessageCallback(const asSMessageInfo* msg, void*) {
    if (msg->type == asMSGTYPE_ERROR) {
        std::fprintf(stderr, "  [msg] %s (%d,%d): %s\n", msg->section, msg->row, msg->col, msg->message);
    }
}

std::string Itos(int v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

std::string Ftos(float v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", (double)v);
    return buf;
}

std::string Dtos(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    return buf;
}

int Add2(int a, int b) {
    return a + b;
}

float Mul2f(float a, float b) {
    return a * b;
}

void Accumulate(int& total, int v) {
    total += v;
}

void RaiseError() {
    asIScriptContext* ctx = asGetActiveContext();
    ctx->SetException("script error", true);
}

void PoisonStack() {
    auto* ctx = static_cast<asCContext*>(asGetActiveContext());
    for (int offset = 1; offset <= 32; offset++)
        ctx->m_regs.stackPointer[-offset] = 1;
}

void RegisterAll(asIScriptEngine* engine) {
    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    RegisterScriptDictionary(engine);

    int r = engine->RegisterGlobalFunction("string itos(int)", asFUNCTION(Itos), asCALL_CDECL);
    if (r < 0) std::printf("itos failed: %d\n", r);
    r = engine->RegisterGlobalFunction("string ftos(float)", asFUNCTION(Ftos), asCALL_CDECL);
    if (r < 0) std::printf("ftos failed: %d\n", r);
    r = engine->RegisterGlobalFunction("string dtos(double)", asFUNCTION(Dtos), asCALL_CDECL);
    if (r < 0) std::printf("dtos failed: %d\n", r);
    r = engine->RegisterGlobalFunction("int add2(int, int)", asFUNCTION(Add2), asCALL_CDECL);
    if (r < 0) std::printf("add2 failed: %d\n", r);
    r = engine->RegisterGlobalFunction("float mul2f(float, float)", asFUNCTION(Mul2f), asCALL_CDECL);
    if (r < 0) std::printf("mul2f failed: %d\n", r);
    r = engine->RegisterGlobalFunction("void accumulate(int&out, int)", asFUNCTION(Accumulate), asCALL_CDECL);
    if (r < 0) std::printf("accumulate failed: %d\n", r);
    r = engine->RegisterGlobalFunction("void RaiseError()", asFUNCTION(RaiseError), asCALL_CDECL);
    if (r < 0) std::printf("RaiseError failed: %d\n", r);
    r = engine->RegisterGlobalFunction("void PoisonStack()", asFUNCTION(PoisonStack), asCALL_CDECL);
    if (r < 0) std::printf("PoisonStack failed: %d\n", r);
}

struct RunResult {
    int         state = -1;
    int         ret = 0;
    std::string out;
    std::string exc;
};

asIScriptModule* BuildModule(asIScriptEngine* engine, const std::string& name, const std::string& code) {
    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, name.c_str());
    if (r < 0) return nullptr;
    r = builder.AddSectionFromMemory(name.c_str(), code.c_str(), (unsigned int)code.size());
    if (r < 0) return nullptr;
    r = builder.BuildModule();
    if (r < 0) return nullptr;

    return builder.GetModule();
}

RunResult RunMain(asIScriptModule* mod) {
    RunResult res;
    if (!mod) return res;

    asIScriptFunction* fn = mod->GetFunctionByName("main");
    if (!fn) return res;

    asIScriptContext* ctx = mod->GetEngine()->CreateContext();
    if (!ctx) return res;
    int r = ctx->Prepare(fn);
    if (r < 0) {
        ctx->Release();
        return res;
    }
    r = ctx->Execute();
    res.state = ctx->GetState();
    res.ret = ctx->GetReturnDWord();
    if (res.state == asEXECUTION_EXCEPTION) {
        const char* s = ctx->GetExceptionString();
        res.exc = s ? s : "";
    }
    ctx->Release();

    int idx = mod->GetGlobalVarIndexByName("g_out");
    if (idx >= 0) {
        void* ptr = mod->GetAddressOfGlobalVar(idx);
        if (ptr) res.out = *static_cast<std::string*>(ptr);
    }

    return res;
}

RunResult BuildAndRun(asIScriptEngine* engine, const std::string& name, const std::string& code) {
    return RunMain(BuildModule(engine, name, code));
}

RunResult BuildPairAndRun(asIScriptEngine* engine,
                          const std::string& providerName,
                          const std::string& providerCode,
                          const std::string& consumerName,
                          const std::string& consumerCode,
                          bool bindImports) {
    if (!BuildModule(engine, providerName, providerCode)) return {};
    asIScriptModule* consumer = BuildModule(engine, consumerName, consumerCode);
    if (!consumer) return {};
    if (bindImports && consumer->BindAllImportedFunctions() < 0) return {};
    return RunMain(consumer);
}

RunResult BuildImportedSystemAndRun(asIScriptEngine* engine,
                                    const std::string& name,
                                    const std::string& code) {
    asIScriptModule* module = BuildModule(engine, name, code);
    if (!module) return {};
    asIScriptFunction* function = engine->GetGlobalFunctionByDecl("int add2(int, int)");
    int index = module->GetImportedFunctionIndexByDecl("int hostAdd(int, int)");
    if (!function || index < 0 || module->BindImportedFunction(static_cast<asUINT>(index), function) < 0)
        return {};
    return RunMain(module);
}

bool LoadFile(const std::string& path, std::string& code) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    code = ss.str();
    return true;
}

void CheckResults(const char* name, const RunResult& ri, const RunResult& rj) {
    if (ri.state < 0 || rj.state < 0) {
        std::printf("build/run setup failed for %s (interp state=%d jit state=%d)\n",
                    name, ri.state, rj.state);
        s_failures++;
        return;
    }

    CHECK_EQ(ri.state, rj.state);
    CHECK_EQ(ri.ret, rj.ret);
    CHECK_EQ(ri.out, rj.out);
    CHECK_EQ(ri.exc, rj.exc);
    if (ri.out != rj.out) {
        std::fprintf(stderr, "%s interpreter output:\n%sJIT output:\n%s",
                     name, ri.out.c_str(), rj.out.c_str());
    }
    std::printf("%-24s ok\n", name);
}

bool ModuleHasJitFunctions(asIScriptModule* mod) {
    for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        asIScriptFunction* f = mod->GetFunctionByIndex(i);
        asUINT len = 0;
        asDWORD* bc = f->GetByteCode(&len);
        if (!bc) continue;
        asDWORD* p = bc;
        asDWORD* end = bc + len;
        while (p < end) {
            asEBCInstr op = static_cast<asEBCInstr>(*p & 0xFF);
            if (op == asBC_JitEntry && *(asPWORD*)(p + 1) != 0)
                return true;
            p += asBCTypeSize[asBCInfo[op].type];
        }
    }
    return false;
}

bool ModuleHasOpcode(asIScriptModule* mod, asEBCInstr expected) {
    for (asUINT i = 0; i < mod->GetFunctionCount(); i++) {
        asIScriptFunction* f = mod->GetFunctionByIndex(i);
        asUINT len = 0;
        asDWORD* bc = f->GetByteCode(&len);
        if (!bc) continue;
        asDWORD* p = bc;
        asDWORD* end = bc + len;
        while (p < end) {
            asEBCInstr op = static_cast<asEBCInstr>(*p & 0xFF);
            if (op == expected) return true;
            p += asBCTypeSize[asBCInfo[op].type];
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    const char* scriptDir = "scripts";
    if (argc > 1) scriptDir = argv[1];

    asIScriptEngine* engineInterp = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    asIScriptEngine* engineJit = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!engineInterp || !engineJit) {
        std::printf("engine creation failed\n");
        return 1;
    }

    engineInterp->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
    engineJit->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);

    void* jit = AsJitCreateEngine(engineJit);
    if (!jit) {
        std::printf("AsJitCreateEngine failed\n");
        return 1;
    }

    RegisterAll(engineInterp);
    RegisterAll(engineJit);

    const char* scripts[] = {
        "arith.as", "branch.as", "funcs.as", "class.as", "sys.as", "except.as", "string.as",
        "globals.as", "statements_extra.as", "types.as", "functions_advanced.as",
        "classes_advanced.as", "operators.as", "handles.as", "lifetime_refs.as",
        "dictionary_handles.as", "shared_mixin.as", "funcptr_fallback.as",
        "funcdef_local_init.as", "object_member_init.as", "simd_float.as",
    };

    bool jitActive = false;
    for (size_t s = 0; s < sizeof(scripts) / sizeof(scripts[0]); s++) {
        std::string path = std::string(scriptDir) + "/" + scripts[s];
        std::string code;
        if (!LoadFile(path, code)) {
            std::printf("cannot open %s\n", path.c_str());
            return 1;
        }

        std::string base = scripts[s];
        base = base.substr(0, base.find('.'));
        std::string interpName = "i_" + base;
        std::string jitName = "j_" + base;
        RunResult ri = BuildAndRun(engineInterp, interpName, code);
        RunResult rj = BuildAndRun(engineJit, jitName, code);
        CheckResults(scripts[s], ri, rj);
        if (base == "string")
            CHECK_TRUE(ModuleHasOpcode(engineJit->GetModule(jitName.c_str()), asBC_Thiscall1));
    }
    {
        std::string provider;
        std::string consumer;
        if (!LoadFile(std::string(scriptDir) + "/imports_provider.as", provider) ||
            !LoadFile(std::string(scriptDir) + "/imports_consumer.as", consumer)) {
            std::printf("cannot open import module scripts\n");
            return 1;
        }
        RunResult ri = BuildPairAndRun(engineInterp, "imports_provider", provider,
                                       "imports_consumer", consumer, true);
        RunResult rj = BuildPairAndRun(engineJit, "imports_provider", provider,
                                       "imports_consumer", consumer, true);
        CheckResults("imports modules", ri, rj);
    }
    {
        const std::string code =
            "string g_out; import int hostAdd(int, int) from 'host'; "
            "int main() { int total = hostAdd(4, 7); g_out += itos(total) + '\\n'; return total; }";
        RunResult ri = BuildImportedSystemAndRun(engineInterp, "i_imported_system", code);
        RunResult rj = BuildImportedSystemAndRun(engineJit, "j_imported_system", code);
        CheckResults("imported system function", ri, rj);
        CHECK_TRUE(ModuleHasOpcode(engineJit->GetModule("j_imported_system"), asBC_CALLBND));
    }
    {
        std::string provider;
        std::string consumer;
        if (!LoadFile(std::string(scriptDir) + "/shared_provider.as", provider) ||
            !LoadFile(std::string(scriptDir) + "/shared_consumer.as", consumer)) {
            std::printf("cannot open shared module scripts\n");
            return 1;
        }
        RunResult ri = BuildPairAndRun(engineInterp, "shared_provider", provider,
                                       "shared_consumer", consumer, false);
        RunResult rj = BuildPairAndRun(engineJit, "shared_provider", provider,
                                       "shared_consumer", consumer, false);
        CheckResults("external shared modules", ri, rj);
    }
    {
        CScriptBuilder probe;
        int pr = probe.StartNewModule(engineJit, "jit_probe");
        pr = probe.AddSectionFromMemory("probe", "int main() { return 1; }");
        pr = probe.BuildModule();
        jitActive = ModuleHasJitFunctions(probe.GetModule());
    }
    CHECK_TRUE(jitActive);

    engineJit->Release();
    AsJitDestroyEngine(jit);
    engineInterp->Release();

    if (s_failures == 0)
        std::printf("ALL %d CHECKS PASSED\n", s_checks);
    else
        std::printf("%d/%d CHECKS FAILED\n", s_failures, s_checks);

    return s_failures == 0 ? 0 : 1;
}
