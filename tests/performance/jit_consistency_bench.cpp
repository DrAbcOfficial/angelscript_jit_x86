#include "as_jit_x86.h"
#include "angelscript.h"
#include "scriptarray.h"
#include "scriptbuilder.h"
#include "scriptdictionary.h"
#include "scriptstdstring.h"
#include "as_context.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#ifndef ASJITX86_SCRIPT_DIR
#define ASJITX86_SCRIPT_DIR "scripts"
#endif

namespace {

constexpr int kDefaultIters = 500000;

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

bool RegisterAll(asIScriptEngine* engine) {
    RegisterStdString(engine);
    RegisterScriptArray(engine, true);
    RegisterScriptDictionary(engine);
    if (engine->RegisterGlobalFunction("string itos(int)", asFUNCTION(Itos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("string ftos(float)", asFUNCTION(Ftos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("string dtos(double)", asFUNCTION(Dtos), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("int add2(int, int)", asFUNCTION(Add2), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("float mul2f(float, float)", asFUNCTION(Mul2f), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("void accumulate(int&out, int)", asFUNCTION(Accumulate), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("void RaiseError()", asFUNCTION(RaiseError), asCALL_CDECL) < 0) return false;
    if (engine->RegisterGlobalFunction("void PoisonStack()", asFUNCTION(PoisonStack), asCALL_CDECL) < 0) return false;
    return true;
}

struct RunResult {
    int         state = -1;
    int         ret = 0;
    std::string out;
    std::string exc;
};

std::string* GlobalOut(asIScriptModule* mod) {
    if (!mod) return nullptr;
    int idx = mod->GetGlobalVarIndexByName("g_out");
    if (idx < 0) return nullptr;
    void* ptr = mod->GetAddressOfGlobalVar(idx);
    return ptr ? static_cast<std::string*>(ptr) : nullptr;
}

asIScriptModule* BuildModule(asIScriptEngine* engine, const std::string& name, const std::string& code) {
    CScriptBuilder builder;
    if (builder.StartNewModule(engine, name.c_str()) < 0) return nullptr;
    if (builder.AddSectionFromMemory(name.c_str(), code.c_str(), (unsigned int)code.size()) < 0) return nullptr;
    if (builder.BuildModule() < 0) return nullptr;
    return builder.GetModule();
}

RunResult RunMain(asIScriptModule* mod) {
    RunResult res;
    if (!mod) return res;
    asIScriptFunction* fn = mod->GetFunctionByName("main");
    if (!fn) return res;
    asIScriptContext* ctx = mod->GetEngine()->CreateContext();
    if (!ctx) return res;
    if (ctx->Prepare(fn) < 0) {
        ctx->Release();
        return res;
    }
    int r = ctx->Execute();
    res.state = ctx->GetState();
    res.ret = ctx->GetReturnDWord();
    if (res.state == asEXECUTION_EXCEPTION) {
        const char* s = ctx->GetExceptionString();
        res.exc = s ? s : "";
    }
    (void)r;
    ctx->Release();
    if (std::string* out = GlobalOut(mod))
        res.out = *out;
    return res;
}

double RepeatMain(asIScriptModule* mod, int iters, int expectedState) {
    if (!mod || iters <= 0) return -1.0;
    asIScriptFunction* fn = mod->GetFunctionByName("main");
    if (!fn) return -1.0;
    asIScriptContext* ctx = mod->GetEngine()->CreateContext();
    if (!ctx) return -1.0;
    std::string* out = GlobalOut(mod);
    auto begin = std::chrono::steady_clock::now();
    for (int i = 0; i < iters; i++) {
        if (out) out->clear();
        if (ctx->Prepare(fn) < 0) {
            ctx->Release();
            return -1.0;
        }
        if (ctx->Execute() != expectedState) {
            ctx->Release();
            return -1.0;
        }
    }
    auto end = std::chrono::steady_clock::now();
    ctx->Release();
    return std::chrono::duration<double, std::milli>(end - begin).count();
}

asIScriptModule* BuildPair(asIScriptEngine* engine,
                           const std::string& providerName,
                           const std::string& providerCode,
                           const std::string& consumerName,
                           const std::string& consumerCode,
                           bool bindImports) {
    if (!BuildModule(engine, providerName, providerCode)) return nullptr;
    asIScriptModule* consumer = BuildModule(engine, consumerName, consumerCode);
    if (!consumer) return nullptr;
    if (bindImports && consumer->BindAllImportedFunctions() < 0) return nullptr;
    return consumer;
}

asIScriptModule* BuildImportedSystem(asIScriptEngine* engine, const std::string& name, const std::string& code) {
    asIScriptModule* module = BuildModule(engine, name, code);
    if (!module) return nullptr;
    asIScriptFunction* function = engine->GetGlobalFunctionByDecl("int add2(int, int)");
    int index = module->GetImportedFunctionIndexByDecl("int hostAdd(int, int)");
    if (!function || index < 0 || module->BindImportedFunction(static_cast<asUINT>(index), function) < 0)
        return nullptr;
    return module;
}

bool LoadFile(const std::string& path, std::string& code) {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    code = ss.str();
    return true;
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

bool IsDigits(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

bool MatchFilter(const char* name, const char* filter) {
    if (!filter) return true;
    return std::strstr(name, filter) != nullptr;
}

bool SameResult(const RunResult& a, const RunResult& b) {
    return a.state >= 0 && b.state >= 0 && a.state == b.state && a.ret == b.ret && a.out == b.out &&
           a.exc == b.exc;
}

struct BenchStats {
    int    failures = 0;
    int    measured = 0;
    int    expected = 0;
    double totalInterp = 0.0;
    double totalJit = 0.0;
    double logSum = 0.0;
    double minSpeedup = (std::numeric_limits<double>::max)();
};

void ReportCase(const char* name, const RunResult& ri, const RunResult& rj, double interpMs, double jitMs,
                BenchStats& stats) {
    bool match = SameResult(ri, rj) && interpMs > 0.0 && jitMs > 0.0;
    double speedup = match ? interpMs / jitMs : 0.0;
    std::printf("%-28s %12.3f %12.3f %8.2fx %s\n", name, interpMs, jitMs, speedup, match ? "ok" : "MISMATCH");
    std::fflush(stdout);
    if (!match) {
        if (ri.state < 0 || rj.state < 0) {
            std::printf("  build/run failed (interp state=%d jit state=%d)\n", ri.state, rj.state);
        } else if (ri.out != rj.out) {
            std::fprintf(stderr, "  interpreter output:\n%s  JIT output:\n%s", ri.out.c_str(), rj.out.c_str());
        } else if (ri.ret != rj.ret || ri.state != rj.state) {
            std::printf("  interp ret=%d state=%d jit ret=%d state=%d\n", ri.ret, ri.state, rj.ret, rj.state);
        }
        stats.failures++;
        return;
    }
    stats.totalInterp += interpMs;
    stats.totalJit += jitMs;
    stats.logSum += std::log(speedup);
    stats.minSpeedup = (std::min)(stats.minSpeedup, speedup);
    stats.measured++;
}

void BenchModules(const char* name, asIScriptModule* interpMod, asIScriptModule* jitMod, int iters,
                  BenchStats& stats) {
    RunResult ri = RunMain(interpMod);
    RunResult rj = RunMain(jitMod);
    double interpMs = RepeatMain(interpMod, iters, ri.state);
    double jitMs = RepeatMain(jitMod, iters, rj.state);
    ReportCase(name, ri, rj, interpMs, jitMs, stats);
}

} // namespace

int main(int argc, char** argv) {
    const char* scriptDir = ASJITX86_SCRIPT_DIR;
    int iters = kDefaultIters;
    const char* filter = nullptr;
    for (int i = 1; i < argc; i++) {
        if (IsDigits(argv[i])) {
            iters = std::atoi(argv[i]);
        } else if (std::strchr(argv[i], '/') || std::strchr(argv[i], '\\')) {
            scriptDir = argv[i];
        } else {
            filter = argv[i];
        }
    }
    if (iters <= 0) {
        std::printf("iterations must be > 0\n");
        return 1;
    }

    asIScriptEngine* engineInterp = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    asIScriptEngine* engineJit = asCreateScriptEngine(ANGELSCRIPT_VERSION);
    if (!engineInterp || !engineJit) {
        std::printf("engine creation failed\n");
        return 1;
    }

    engineInterp->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    engineJit->SetEngineProperty(asEP_BUILD_WITHOUT_LINE_CUES, true);
    engineInterp->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);
    engineJit->SetMessageCallback(asFUNCTION(MessageCallback), nullptr, asCALL_CDECL);

    void* jit = AsJitCreateEngine(engineJit);
    if (!jit) {
        std::printf("AsJitCreateEngine failed\n");
        return 1;
    }
    if (!RegisterAll(engineInterp) || !RegisterAll(engineJit)) {
        std::printf("host registration failed\n");
        return 1;
    }

    {
        asIScriptModule* probe = BuildModule(engineJit, "jit_probe", "int main() { return 1; }");
        if (!probe || !ModuleHasJitFunctions(probe)) {
            std::printf("JIT probe failed\n");
            return 1;
        }
    }

    const char* scripts[] = {
        "arith.as", "branch.as", "funcs.as", "class.as", "sys.as", "except.as", "string.as",
        "globals.as", "statements_extra.as", "types.as", "functions_advanced.as",
        "classes_advanced.as", "operators.as", "handles.as", "lifetime_refs.as",
        "dictionary_handles.as", "shared_mixin.as", "funcptr_fallback.as",
        "funcdef_local_init.as", "object_member_init.as", "simd_float.as",
    };

    std::printf("iters=%d script_dir=%s\n", iters, scriptDir);
    std::printf("%-28s %12s %12s %9s %s\n", "case", "interp ms", "jit ms", "speedup", "check");
    std::fflush(stdout);

    BenchStats stats;

    for (size_t s = 0; s < sizeof(scripts) / sizeof(scripts[0]); s++) {
        if (!MatchFilter(scripts[s], filter)) continue;
        stats.expected++;
        std::string path = std::string(scriptDir) + "/" + scripts[s];
        std::string code;
        if (!LoadFile(path, code)) {
            std::printf("%-28s cannot open %s\n", scripts[s], path.c_str());
            stats.failures++;
            continue;
        }
        std::string base = scripts[s];
        base = base.substr(0, base.find('.'));
        asIScriptModule* interpMod = BuildModule(engineInterp, "i_" + base, code);
        asIScriptModule* jitMod = BuildModule(engineJit, "j_" + base, code);
        BenchModules(scripts[s], interpMod, jitMod, iters, stats);
    }

    if (MatchFilter("imports modules", filter) || MatchFilter("imports_consumer.as", filter)) {
        stats.expected++;
        std::string provider;
        std::string consumer;
        if (!LoadFile(std::string(scriptDir) + "/imports_provider.as", provider) ||
            !LoadFile(std::string(scriptDir) + "/imports_consumer.as", consumer)) {
            std::printf("%-28s cannot open import module scripts\n", "imports modules");
            stats.failures++;
        } else {
            asIScriptModule* interpMod =
                BuildPair(engineInterp, "imports_provider", provider, "imports_consumer", consumer, true);
            asIScriptModule* jitMod =
                BuildPair(engineJit, "imports_provider", provider, "imports_consumer", consumer, true);
            BenchModules("imports modules", interpMod, jitMod, iters, stats);
        }
    }

    if (MatchFilter("imported system function", filter)) {
        stats.expected++;
        const std::string code =
            "string g_out; import int hostAdd(int, int) from 'host'; "
            "int main() { int total = hostAdd(4, 7); g_out += itos(total) + '\\n'; return total; }";
        asIScriptModule* interpMod = BuildImportedSystem(engineInterp, "i_imported_system", code);
        asIScriptModule* jitMod = BuildImportedSystem(engineJit, "j_imported_system", code);
        BenchModules("imported system function", interpMod, jitMod, iters, stats);
    }

    if (MatchFilter("external shared modules", filter) || MatchFilter("shared_consumer.as", filter)) {
        stats.expected++;
        std::string provider;
        std::string consumer;
        if (!LoadFile(std::string(scriptDir) + "/shared_provider.as", provider) ||
            !LoadFile(std::string(scriptDir) + "/shared_consumer.as", consumer)) {
            std::printf("%-28s cannot open shared module scripts\n", "external shared modules");
            stats.failures++;
        } else {
            asIScriptModule* interpMod =
                BuildPair(engineInterp, "shared_provider", provider, "shared_consumer", consumer, false);
            asIScriptModule* jitMod =
                BuildPair(engineJit, "shared_provider", provider, "shared_consumer", consumer, false);
            BenchModules("external shared modules", interpMod, jitMod, iters, stats);
        }
    }

    std::printf("%-28s %12.3f %12.3f %8.2fx\n", "TOTAL", stats.totalInterp, stats.totalJit,
                stats.totalJit > 0.0 ? stats.totalInterp / stats.totalJit : 0.0);
    if (stats.measured > 0) {
        std::printf("cases=%d geomean=%.2fx min=%.2fx iters=%d\n", stats.measured,
                    std::exp(stats.logSum / stats.measured), stats.minSpeedup, iters);
    }

    engineJit->Release();
    AsJitDestroyEngine(jit);
    engineInterp->Release();

    bool pass = stats.failures == 0 && stats.measured == stats.expected && stats.expected > 0;
    std::printf(pass ? "CONSISTENCY BENCH PASSED\n" : "CONSISTENCY BENCH FAILED\n");
    return pass ? 0 : 1;
}
