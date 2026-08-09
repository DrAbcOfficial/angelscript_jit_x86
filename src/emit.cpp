#include "emit.h"
#include "bc_helpers.h"
#include "bc_info.h"

#include <asmjit/x86.h>

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace asjitx86 {

namespace {

struct EmitIns {
    asEBCInstr op;
    uint32_t   off;
    uint32_t   size;
};

}

int EmitFunction(asmjit::JitRuntime& runtime, asIScriptFunction* function, asJITFunction* out) {
    using namespace asmjit;

    *out = nullptr;

    asUINT bcLen = 0;
    asDWORD* bc = function->GetByteCode(&bcLen);
    if (!bc || bcLen == 0) return asERROR;

    std::vector<EmitIns> ins;
    std::unordered_map<uint32_t, size_t> indexOfOffset;
    uint32_t off = 0;
    while (off < bcLen) {
        asEBCInstr op = static_cast<asEBCInstr>(bc[off] & 0xFF);
        int sz = BcSize(op);
        if (sz <= 0) return asERROR;
        indexOfOffset[off] = ins.size();
        ins.push_back(EmitIns{op, off, static_cast<uint32_t>(sz)});
        off += static_cast<uint32_t>(sz);
    }
    if (off != bcLen) return asERROR;

    CodeHolder code;
    Error err = code.init(runtime.environment(), runtime.cpu_features());
    if (err != kErrorOk) return asERROR;

    x86::Compiler cc(&code);
    FuncNode* fnNode = cc.add_func(FuncSignature::build<void, asSVMRegisters*, asPWORD>());
    if (!fnNode) return asERROR;

    x86::Gp regs = cc.new_gp32("regs");
    fnNode->set_arg(0, regs);

    std::vector<Label> labels;
    labels.reserve(ins.size());
    for (size_t i = 0; i < ins.size(); i++) labels.push_back(cc.new_label());
    Label exitLabel = cc.new_label();

    {
        x86::Gp pp = cc.new_gp32("pp");
        cc.mov(pp, x86::dword_ptr(regs, offsetof(asSVMRegisters, programPointer)));
        for (size_t i = 0; i < ins.size(); i++) {
            if (ins[i].op == asBC_JitEntry) {
                cc.cmp(pp, Imm(int64_t((intptr_t)(bc + ins[i].off))));
                cc.je(labels[i]);
            }
        }
        cc.jmp(exitLabel);
    }

    for (size_t i = 0; i < ins.size(); i++) {
        cc.bind(labels[i]);
        const EmitIns& in = ins[i];

        switch (in.op) {
        case asBC_JMP: {
            int64_t target = int64_t(in.off) + 2 + asBC_INTARG(bc + in.off);
            auto it = indexOfOffset.find(static_cast<uint32_t>(target));
            if (it == indexOfOffset.end()) return asERROR;
            cc.jmp(labels[it->second]);
            break;
        }
        case asBC_JZ:
        case asBC_JNZ:
        case asBC_JS:
        case asBC_JNS:
        case asBC_JP:
        case asBC_JNP:
        case asBC_JLowZ:
        case asBC_JLowNZ: {
            int64_t target = int64_t(in.off) + 2 + asBC_INTARG(bc + in.off);
            auto it = indexOfOffset.find(static_cast<uint32_t>(target));
            if (it == indexOfOffset.end()) return asERROR;
            if (in.op == asBC_JLowZ || in.op == asBC_JLowNZ)
                cc.cmp(x86::byte_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            else
                cc.cmp(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            switch (in.op) {
            case asBC_JZ:     cc.jz(labels[it->second]); break;
            case asBC_JNZ:    cc.jnz(labels[it->second]); break;
            case asBC_JS:     cc.js(labels[it->second]); break;
            case asBC_JNS:    cc.jns(labels[it->second]); break;
            case asBC_JP:     cc.jg(labels[it->second]); break;
            case asBC_JNP:    cc.jle(labels[it->second]); break;
            case asBC_JLowZ:  cc.jz(labels[it->second]); break;
            case asBC_JLowNZ: cc.jnz(labels[it->second]); break;
            default: break;
            }
            break;
        }
        default: {
            InvokeNode* inv = nullptr;
            err = cc.invoke(Out<InvokeNode*>(inv),
                            Imm(int64_t((intptr_t)&JitBcFallback)),
                            FuncSignature::build<int, asSVMRegisters*, const asDWORD*>());
            if (err != kErrorOk) return asERROR;
            x86::Gp res = cc.new_gp32("res");
            inv->set_arg(0, regs);
            inv->set_arg(1, Imm(int64_t((intptr_t)(bc + in.off))));
            inv->set_ret(0, res);
            cc.test(res, res);
            cc.jnz(exitLabel);
            break;
        }
        }
    }

    cc.bind(exitLabel);
    cc.end_func();
    err = cc.finalize();
    if (err != kErrorOk) return asERROR;

    asJITFunction fn = nullptr;
    err = runtime.add(&fn, &code);
    if (err != kErrorOk) return asERROR;

    for (size_t i = 0; i < ins.size(); i++) {
        if (ins[i].op == asBC_JitEntry)
            *(asPWORD*)(bc + ins[i].off + 1) = asPWORD(bc + ins[i].off);
    }

    *out = fn;
    return asSUCCESS;
}

}
