#include "as_jit_x86.h"
#include "angelscript.h"
#include "scriptbuilder.h"
#include "scriptstdstring.h"

#include <cstdio>
#include <cstring>
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

void MessageCallback(const asSMessageInfo* msg, void* param) {
    int* errs = static_cast<int*>(param);
    if (msg->type == asMSGTYPE_ERROR) {
        (*errs)++;
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

void RegisterAll(asIScriptEngine* engine) {
    RegisterStdString(engine);

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
}

struct RunResult {
    int         state = -1;
    int         ret = 0;
    std::string out;
    std::string exc;
};

RunResult BuildAndRun(asIScriptEngine* engine, const std::string& name, const std::string& code) {
    RunResult res;

    CScriptBuilder builder;
    int r = builder.StartNewModule(engine, name.c_str());
if (r < 0) return res;
    r = builder.AddSectionFromMemory(name.c_str(), code.c_str(), (unsigned int)code.size());
if (r < 0) return res;
    r = builder.BuildModule();
if (r < 0) return res;

    asIScriptModule* mod = builder.GetModule();
    asIScriptFunction* fn = mod->GetFunctionByName("main");
if (!fn) return res;

    asIScriptContext* ctx = engine->CreateContext();
    if (!ctx) return res;
    r = ctx->Prepare(fn);
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

    int errs = 0;
    engineInterp->SetMessageCallback(asFUNCTION(MessageCallback), &errs, asCALL_CDECL);
    engineJit->SetMessageCallback(asFUNCTION(MessageCallback), &errs, asCALL_CDECL);

    void* jit = AsJitCreateEngine(engineJit);
    if (!jit) {
        std::printf("AsJitCreateEngine failed\n");
        return 1;
    }

    RegisterAll(engineInterp);
    RegisterAll(engineJit);

    const char* scripts[] = {
        "arith.as", "branch.as", "funcs.as", "class.as", "sys.as", "except.as", "string.as",
    };

    bool jitActive = false;
    for (size_t s = 0; s < sizeof(scripts) / sizeof(scripts[0]); s++) {
        std::string path = std::string(scriptDir) + "/" + scripts[s];
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in) {
            std::printf("cannot open %s\n", path.c_str());
            return 1;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        std::string code = ss.str();

        std::string base = scripts[s];
        base = base.substr(0, base.find('.'));
        RunResult ri = BuildAndRun(engineInterp, ("i_" + base).c_str(), code);
        RunResult rj = BuildAndRun(engineJit, ("j_" + base).c_str(), code);
        if (strcmp(scripts[s], "class.as") == 0)

        if (ri.state < 0 || rj.state < 0) {
            std::printf("build/run setup failed for %s (interp state=%d jit state=%d)\n",
                        scripts[s], ri.state, rj.state);
            s_failures++;
            continue;
        }

        CHECK_EQ(ri.state, rj.state);
        CHECK_EQ(ri.ret, rj.ret);
        CHECK_EQ(ri.out, rj.out);
        CHECK_EQ(ri.exc, rj.exc);
        std::printf("%-12s ok\n", scripts[s]);
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
