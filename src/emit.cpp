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

constexpr bool kInlinePshC4   = true;
constexpr bool kInlinePshV4   = true;
constexpr bool kInlinePsf     = true;
constexpr bool kInlineAdd6b   = true;
constexpr bool kInlineSub6b   = true;
constexpr bool kInlineMul6b   = true;
constexpr bool kInlineDiv6b   = true;
constexpr bool kInlineBits6b  = true;
constexpr bool kInlineNeg6b   = true;
constexpr bool kInlineNot6b   = true;
constexpr bool kInlineCmp6c   = true;

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
        const asDWORD* ip = bc + in.off;

        const uint32_t ppOff = offsetof(asSVMRegisters, programPointer);
        const uint32_t fpOff = offsetof(asSVMRegisters, stackFramePointer);
        const uint32_t spOff = offsetof(asSVMRegisters, stackPointer);

        auto loadVar = [&](int offset, const x86::Gp& dst) {
            cc.mov(dst, x86::dword_ptr(regs, fpOff));
            cc.mov(dst, x86::dword_ptr(dst, -offset * 4));
        };
        auto storeVar = [&](int offset, const x86::Gp& src) {
            x86::Gp tmp = cc.new_gp32("fp");
            cc.mov(tmp, x86::dword_ptr(regs, fpOff));
            cc.mov(x86::dword_ptr(tmp, -offset * 4), src);
        };
        auto loadSp = [&](const x86::Gp& dst) {
            cc.mov(dst, x86::dword_ptr(regs, spOff));
        };
        auto storeSp = [&](const x86::Gp& src) {
            cc.mov(x86::dword_ptr(regs, spOff), src);
        };
        auto storeProgramPointer = [&](const asDWORD* next) {
            cc.mov(x86::dword_ptr(regs, ppOff), Imm(int64_t((intptr_t)next)));
        };
        auto emitHelperCall = [&]() -> bool {
            InvokeNode* inv = nullptr;
            Error invErr = cc.invoke(Out<InvokeNode*>(inv),
                                     Imm(int64_t((intptr_t)&JitBcFallback)),
                                     FuncSignature::build<int, asSVMRegisters*, const asDWORD*>());
            if (invErr != kErrorOk) return false;
            x86::Gp res = cc.new_gp32("res");
            inv->set_arg(0, regs);
            inv->set_arg(1, Imm(int64_t((intptr_t)ip)));
            inv->set_ret(0, res);
            cc.test(res, res);
            cc.jnz(exitLabel);
            return true;
        };

        switch (in.op) {
        case asBC_JMP: {
            int64_t target = int64_t(in.off) + 2 + asBC_INTARG(ip);
            auto it = indexOfOffset.find(static_cast<uint32_t>(target));
            if (it == indexOfOffset.end()) return asERROR;
            storeProgramPointer(bc + target);
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
            int64_t target = int64_t(in.off) + 2 + asBC_INTARG(ip);
            auto it = indexOfOffset.find(static_cast<uint32_t>(target));
            if (it == indexOfOffset.end()) return asERROR;
            if (in.op == asBC_JLowZ || in.op == asBC_JLowNZ)
                cc.cmp(x86::byte_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            else
                cc.cmp(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            storeProgramPointer(bc + target);
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
            storeProgramPointer(ip + in.size);
            break;
        }
        case asBC_PshC4: {
            if (kInlinePshC4) {
                x86::Gp sp = cc.new_gp32("sp");
                loadSp(sp);
                cc.sub(sp, 4);
                cc.mov(x86::dword_ptr(sp), Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
                storeSp(sp);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_PshV4: {
            if (kInlinePshV4) {
                int offset = asBC_SWORDARG0(ip);
                x86::Gp sp = cc.new_gp32("sp");
                x86::Gp v = cc.new_gp32("v");
                loadSp(sp);
                cc.sub(sp, 4);
                loadVar(offset, v);
                cc.mov(x86::dword_ptr(sp), v);
                storeSp(sp);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_PSF: {
            if (kInlinePsf) {
                int offset = asBC_SWORDARG0(ip);
                x86::Gp sp = cc.new_gp32("sp");
                x86::Gp v = cc.new_gp32("v");
                loadSp(sp);
                cc.sub(sp, 4);
                cc.mov(v, x86::dword_ptr(regs, fpOff));
                cc.lea(v, x86::dword_ptr(v, -offset * 4));
                cc.mov(x86::dword_ptr(sp), v);
                storeSp(sp);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_ADDi:
        case asBC_SUBi:
        case asBC_MULi: {
            const bool shouldInline = (in.op == asBC_ADDi && kInlineAdd6b) ||
                                      (in.op == asBC_SUBi && kInlineSub6b) ||
                                      (in.op == asBC_MULi && kInlineMul6b);
            if (shouldInline) {
                int a0 = asBC_SWORDARG0(ip);
                int a1 = asBC_SWORDARG1(ip);
                int a2 = asBC_SWORDARG2(ip);
                x86::Gp x = cc.new_gp32("x");
                x86::Gp y = cc.new_gp32("y");
                loadVar(a1, x);
                loadVar(a2, y);
                switch (in.op) {
                case asBC_ADDi: cc.add(x, y); break;
                case asBC_SUBi: cc.sub(x, y); break;
                default:        cc.imul(x, y); break;
                }
                storeVar(a0, x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_DIVi:
        case asBC_MODi: {
            if (kInlineDiv6b) {
                int a0 = asBC_SWORDARG0(ip);
                int a1 = asBC_SWORDARG1(ip);
                int a2 = asBC_SWORDARG2(ip);
                x86::Gp dividend = cc.new_gp32("dividend");
                x86::Gp divisor = cc.new_gp32("divisor");
                x86::Gp high = cc.new_gp32("high");
                Label overflowCheck = cc.new_label();
                Label fallback = cc.new_label();
                Label divide = cc.new_label();
                Label done = cc.new_label();
                loadVar(a1, dividend);
                loadVar(a2, divisor);
                cc.test(divisor, divisor);
                cc.jz(fallback);
                cc.cmp(divisor, -1);
                cc.je(overflowCheck);
                cc.jmp(divide);
                cc.bind(overflowCheck);
                cc.cmp(dividend, Imm(int64_t(INT32_MIN)));
                cc.je(fallback);
                cc.bind(divide);
                cc.mov(high, dividend);
                cc.sar(high, 31);
                cc.idiv(high, dividend, divisor);
                storeVar(a0, in.op == asBC_DIVi ? dividend : high);
                cc.jmp(done);
                cc.bind(fallback);
                if (!emitHelperCall()) return asERROR;
                cc.bind(done);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_BAND:
        case asBC_BOR:
        case asBC_BXOR:
        case asBC_BSLL:
        case asBC_BSRL:
        case asBC_BSRA: {
            if (kInlineBits6b) {
                int a0 = asBC_SWORDARG0(ip);
                int a1 = asBC_SWORDARG1(ip);
                int a2 = asBC_SWORDARG2(ip);
                x86::Gp x = cc.new_gp32("x");
                x86::Gp y = cc.new_gp32("y");
                loadVar(a1, x);
                loadVar(a2, y);
                switch (in.op) {
                case asBC_BAND: cc.and_(x, y); break;
                case asBC_BOR:  cc.or_(x, y); break;
                case asBC_BXOR: cc.xor_(x, y); break;
                case asBC_BSLL: cc.shl(x, y); break;
                case asBC_BSRL: cc.shr(x, y); break;
                default:        cc.sar(x, y); break;
                }
                storeVar(a0, x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_NEGi: {
            if (kInlineNeg6b) {
                int a0 = asBC_SWORDARG0(ip);
                x86::Gp x = cc.new_gp32("x");
                loadVar(a0, x);
                cc.neg(x);
                storeVar(a0, x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_NOT: {
            if (kInlineNot6b) {
                int a0 = asBC_SWORDARG0(ip);
                x86::Gp x = cc.new_gp32("x");
                cc.mov(x, x86::dword_ptr(regs, fpOff));
                cc.movzx(x, x86::byte_ptr(x, -a0 * 4));
                cc.test(x, x);
                cc.set(x86::CondCode::kEqual, x);
                cc.movzx(x, x.r8());
                storeVar(a0, x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CMPi: {
            if (kInlineCmp6c) {
                int a0 = asBC_SWORDARG0(ip);
                int a1 = asBC_SWORDARG1(ip);
                x86::Gp x = cc.new_gp32("x");
                x86::Gp y = cc.new_gp32("y");
                loadVar(a0, x);
                loadVar(a1, y);
                cc.cmp(x, y);
                cc.set(x86::CondCode::kSignedGT, x);
                cc.set(x86::CondCode::kSignedLT, y);
                cc.movzx(x, x.r8());
                cc.movzx(y, y.r8());
                cc.sub(x, y);
                cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_JitEntry:
            storeProgramPointer(ip + in.size);
            break;
        case asBC_SUSPEND: {
            Label process = cc.new_label();
            Label done = cc.new_label();
            cc.cmp(x86::byte_ptr(regs, offsetof(asSVMRegisters, doProcessSuspend)), 0);
            cc.jne(process);
            storeProgramPointer(ip + in.size);
            cc.jmp(done);
            cc.bind(process);
            if (!emitHelperCall()) return asERROR;
            cc.bind(done);
            break;
        }
        default: {
            if (!emitHelperCall()) return asERROR;
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
