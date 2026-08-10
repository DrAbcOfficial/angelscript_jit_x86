#include "codegen/emit.h"
#include "bytecode/bc_helpers.h"
#include "bytecode/bc_info.h"

#include <asmjit/x86.h>

#include <cstddef>
#include <vector>

namespace asjitx86 {

namespace {

struct EmitIns {
    asEBCInstr op;
    uint32_t   off;
    uint32_t   size;
};

bool IsConditionalBranch(asEBCInstr op) {
    switch (op) {
    case asBC_JZ:
    case asBC_JNZ:
    case asBC_JS:
    case asBC_JNS:
    case asBC_JP:
    case asBC_JNP:
    case asBC_JLowZ:
    case asBC_JLowNZ:
        return true;
    default:
        return false;
    }
}

bool WritesValueRegister(asEBCInstr op) {
    switch (op) {
    case asBC_LdGRdR4:
    case asBC_CMPd:
    case asBC_CMPu:
    case asBC_CMPf:
    case asBC_CMPi:
    case asBC_CMPIi:
    case asBC_CMPIf:
    case asBC_CMPIu:
    case asBC_PopRPtr:
    case asBC_CpyVtoR4:
    case asBC_CpyVtoR8:
    case asBC_LDG:
    case asBC_LDV:
    case asBC_CmpPtr:
    case asBC_CMPi64:
    case asBC_CMPu64:
    case asBC_LoadThisR:
    case asBC_LoadRObjR:
    case asBC_LoadVObjR:
        return true;
    default:
        return false;
    }
}

bool PreservesValueRegister(asEBCInstr op) {
    switch (op) {
    case asBC_PopPtr:
    case asBC_PshGPtr:
    case asBC_PshC4:
    case asBC_PshV4:
    case asBC_PSF:
    case asBC_SwapPtr:
    case asBC_NOT:
    case asBC_PshG4:
    case asBC_NEGi:
    case asBC_NEGf:
    case asBC_NEGd:
    case asBC_IncVi:
    case asBC_DecVi:
    case asBC_BNOT:
    case asBC_BAND:
    case asBC_BOR:
    case asBC_BXOR:
    case asBC_BSLL:
    case asBC_BSRL:
    case asBC_BSRA:
    case asBC_PshC8:
    case asBC_PshVPtr:
    case asBC_SetV4:
    case asBC_SetV8:
    case asBC_SetV1:
    case asBC_SetV2:
    case asBC_CpyVtoV4:
    case asBC_CpyVtoV8:
    case asBC_CpyVtoG4:
    case asBC_CpyGtoV4:
    case asBC_iTOf:
    case asBC_fTOi:
    case asBC_uTOf:
    case asBC_fTOu:
    case asBC_sbTOi:
    case asBC_swTOi:
    case asBC_ubTOi:
    case asBC_uwTOi:
    case asBC_dTOi:
    case asBC_dTOu:
    case asBC_dTOf:
    case asBC_iTOd:
    case asBC_uTOd:
    case asBC_fTOd:
    case asBC_ADDi:
    case asBC_SUBi:
    case asBC_MULi:
    case asBC_ADDf:
    case asBC_SUBf:
    case asBC_MULf:
    case asBC_ADDd:
    case asBC_SUBd:
    case asBC_MULd:
    case asBC_ADDIi:
    case asBC_SUBIi:
    case asBC_MULIi:
    case asBC_ADDIf:
    case asBC_SUBIf:
    case asBC_MULIf:
    case asBC_SetG4:
    case asBC_iTOb:
    case asBC_iTOw:
    case asBC_i64TOi:
    case asBC_uTOi64:
    case asBC_iTOi64:
    case asBC_NEGi64:
    case asBC_BNOT64:
    case asBC_ADDi64:
    case asBC_SUBi64:
    case asBC_MULi64:
    case asBC_BAND64:
    case asBC_BOR64:
    case asBC_BXOR64:
    case asBC_PshV8:
    case asBC_JitEntry:
    case asBC_PshNull:
    case asBC_ClrVPtr:
    case asBC_OBJTYPE:
    case asBC_TYPEID:
    case asBC_FuncPtr:
        return true;
    default:
        return false;
    }
}

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
constexpr bool kInlineIncDecV = true;
constexpr bool kInlineImmInt  = true;
constexpr bool kInlineCmp6c   = true;
constexpr bool kFuseCmpBranch = true;

}

int EmitFunction(asmjit::JitRuntime& runtime, asIScriptFunction* function, asJITFunction* out) {
    using namespace asmjit;

    *out = nullptr;

    asUINT bcLen = 0;
    asDWORD* bc = function->GetByteCode(&bcLen);
    if (!bc || bcLen == 0) return asERROR;

    std::vector<EmitIns> ins;
    ins.reserve(bcLen);
    std::vector<int> indexOfOffset(bcLen, -1);
    uint32_t off = 0;
    while (off < bcLen) {
        asEBCInstr op = static_cast<asEBCInstr>(bc[off] & 0xFF);
        int sz = BcSize(op);
        if (sz <= 0) return asERROR;
        indexOfOffset[off] = static_cast<int>(ins.size());
        ins.push_back(EmitIns{op, off, static_cast<uint32_t>(sz)});
        off += static_cast<uint32_t>(sz);
    }
    if (off != bcLen) return asERROR;

    std::vector<uint8_t> needsLabel(ins.size(), 0);
    for (size_t i = 0; i < ins.size(); i++) {
        const EmitIns& in = ins[i];
        const asDWORD* ip = bc + in.off;
        if (in.op == asBC_JitEntry) {
            needsLabel[i] = 1;
            continue;
        }
        switch (in.op) {
        case asBC_JMP:
        case asBC_JZ:
        case asBC_JNZ:
        case asBC_JS:
        case asBC_JNS:
        case asBC_JP:
        case asBC_JNP:
        case asBC_JLowZ:
        case asBC_JLowNZ: {
            int64_t target = int64_t(in.off) + 2 + asBC_INTARG(ip);
            if (target < 0 || target >= int64_t(bcLen)) return asERROR;
            int targetIndex = indexOfOffset[static_cast<size_t>(target)];
            if (targetIndex < 0) return asERROR;
            needsLabel[static_cast<size_t>(targetIndex)] = 1;
            break;
        }
        default:
            break;
        }
    }

    auto valueRegisterDeadFrom = [&](size_t start) {
        std::vector<uint8_t> visited(ins.size(), 0);
        size_t current = start;
        while (current < ins.size()) {
            if (visited[current]) return false;
            visited[current] = 1;
            const EmitIns& currentIns = ins[current];
            if (WritesValueRegister(currentIns.op)) return true;
            if (currentIns.op == asBC_JMP) {
                int64_t target = int64_t(currentIns.off) + 2 + asBC_INTARG(bc + currentIns.off);
                if (target < 0 || target >= int64_t(bcLen)) return false;
                int targetIndex = indexOfOffset[static_cast<size_t>(target)];
                if (targetIndex < 0) return false;
                current = static_cast<size_t>(targetIndex);
                continue;
            }
            if (!PreservesValueRegister(currentIns.op)) return false;
            current++;
        }
        return false;
    };

    std::vector<uint8_t> fusedCmpBranch(ins.size(), 0);
    std::vector<int8_t> fusedFallValue(ins.size(), 2);
    if (kFuseCmpBranch && kInlineCmp6c) {
        for (size_t i = 0; i + 2 < ins.size(); i++) {
            if (ins[i].op != asBC_CMPi || !IsConditionalBranch(ins[i + 1].op) || needsLabel[i + 1])
                continue;
            const EmitIns& branch = ins[i + 1];
            int64_t target = int64_t(branch.off) + 2 + asBC_INTARG(bc + branch.off);
            if (target < 0 || target >= int64_t(bcLen)) return asERROR;
            int targetIndex = indexOfOffset[static_cast<size_t>(target)];
            if (targetIndex < 0) return asERROR;
            bool fallDead = valueRegisterDeadFrom(i + 2);
            bool takenDead = valueRegisterDeadFrom(static_cast<size_t>(targetIndex));
            int fallValue = 2;
            switch (branch.op) {
            case asBC_JNZ:
            case asBC_JLowNZ:
                fallValue = 0;
                break;
            case asBC_JNS:
                fallValue = -1;
                break;
            case asBC_JNP:
                fallValue = 1;
                break;
            default:
                break;
            }
            if (takenDead && (fallDead || fallValue != 2)) {
                fusedCmpBranch[i] = 1;
                if (!fallDead) fusedFallValue[i] = static_cast<int8_t>(fallValue);
            }
        }
    }

    CodeHolder code;
    Error err = code.init(runtime.environment(), runtime.cpu_features());
    if (err != kErrorOk) return asERROR;

    x86::Compiler cc(&code);
    FuncNode* fnNode = cc.add_func(FuncSignature::build<void, asSVMRegisters*, asPWORD>());
    if (!fnNode) return asERROR;

    x86::Gp regs = cc.new_gp32("regs");
    x86::Gp jitArg = cc.new_gp32("jitArg");
    fnNode->set_arg(0, regs);
    fnNode->set_arg(1, jitArg);

    const uint32_t ppOff = offsetof(asSVMRegisters, programPointer);
    const uint32_t fpOff = offsetof(asSVMRegisters, stackFramePointer);
    const uint32_t spOff = offsetof(asSVMRegisters, stackPointer);

    x86::Gp fp = cc.new_gp32("fp");
    cc.mov(fp, x86::dword_ptr(regs, fpOff));

    std::vector<Label> labels;
    labels.reserve(ins.size());
    labels.resize(ins.size());
    for (size_t i = 0; i < ins.size(); i++) {
        if (needsLabel[i]) labels[i] = cc.new_label();
    }
    Label exitLabel = cc.new_label();

    {
        asPWORD entryId = 1;
        for (size_t i = 0; i < ins.size(); i++) {
            if (ins[i].op == asBC_JitEntry) {
                cc.cmp(jitArg, Imm(entryId++));
                cc.je(labels[i]);
            }
        }
        cc.jmp(exitLabel);
    }

    for (size_t i = 0; i < ins.size(); i++) {
        if (i > 0 && fusedCmpBranch[i - 1]) continue;
        if (needsLabel[i]) cc.bind(labels[i]);
        const EmitIns& in = ins[i];
        const asDWORD* ip = bc + in.off;

        auto loadVar = [&](int offset, const x86::Gp& dst) {
            cc.mov(dst, x86::dword_ptr(fp, -offset * 4));
        };
        auto storeVar = [&](int offset, const x86::Gp& src) {
            cc.mov(x86::dword_ptr(fp, -offset * 4), src);
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
            JitBcHelper helper = GetJitBcHelper(in.op);
            if (!helper) return false;
            InvokeNode* inv = nullptr;
            Error invErr = cc.invoke(Out<InvokeNode*>(inv),
                                     Imm(int64_t((intptr_t)helper)),
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
            if (target < 0 || target >= int64_t(bcLen)) return asERROR;
            int targetIndex = indexOfOffset[static_cast<size_t>(target)];
            if (targetIndex < 0) return asERROR;
            storeProgramPointer(bc + target);
            cc.jmp(labels[static_cast<size_t>(targetIndex)]);
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
            if (target < 0 || target >= int64_t(bcLen)) return asERROR;
            int targetIndex = indexOfOffset[static_cast<size_t>(target)];
            if (targetIndex < 0) return asERROR;
            if (in.op == asBC_JLowZ || in.op == asBC_JLowNZ)
                cc.cmp(x86::byte_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            else
                cc.cmp(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), 0);
            storeProgramPointer(bc + target);
            switch (in.op) {
            case asBC_JZ:     cc.jz(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JNZ:    cc.jnz(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JS:     cc.js(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JNS:    cc.jns(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JP:     cc.jg(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JNP:    cc.jle(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JLowZ:  cc.jz(labels[static_cast<size_t>(targetIndex)]); break;
            case asBC_JLowNZ: cc.jnz(labels[static_cast<size_t>(targetIndex)]); break;
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
                cc.lea(v, x86::dword_ptr(fp, -offset * 4));
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
                cc.movzx(x, x86::byte_ptr(fp, -a0 * 4));
                cc.test(x, x);
                cc.set(x86::CondCode::kEqual, x);
                cc.movzx(x, x.r8());
                storeVar(a0, x);
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_IncVi:
        case asBC_DecVi: {
            if (kInlineIncDecV) {
                int a0 = asBC_SWORDARG0(ip);
                if (in.op == asBC_IncVi)
                    cc.inc(x86::dword_ptr(fp, -a0 * 4));
                else
                    cc.dec(x86::dword_ptr(fp, -a0 * 4));
                storeProgramPointer(ip + in.size);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_ADDIi:
        case asBC_SUBIi:
        case asBC_MULIi: {
            if (kInlineImmInt) {
                int a0 = asBC_SWORDARG0(ip);
                int a1 = asBC_SWORDARG1(ip);
                int32_t value = asBC_INTARG(ip + 1);
                x86::Gp x = cc.new_gp32("x");
                loadVar(a1, x);
                switch (in.op) {
                case asBC_ADDIi: cc.add(x, value); break;
                case asBC_SUBIi: cc.sub(x, value); break;
                default:         cc.imul(x, x, value); break;
                }
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
                if (fusedCmpBranch[i]) {
                    const EmitIns& branch = ins[i + 1];
                    const asDWORD* branchIp = bc + branch.off;
                    int64_t target = int64_t(branch.off) + 2 + asBC_INTARG(branchIp);
                    int targetIndex = indexOfOffset[static_cast<size_t>(target)];
                    storeProgramPointer(ip + in.size);
                    storeProgramPointer(bc + target);
                    switch (branch.op) {
                    case asBC_JZ:     cc.jz(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JNZ:    cc.jnz(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JS:     cc.js(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JNS:    cc.jns(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JP:     cc.jg(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JNP:    cc.jle(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JLowZ:  cc.jz(labels[static_cast<size_t>(targetIndex)]); break;
                    case asBC_JLowNZ: cc.jnz(labels[static_cast<size_t>(targetIndex)]); break;
                    default: return asERROR;
                    }
                    if (fusedFallValue[i] != 2)
                        cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)),
                               fusedFallValue[i]);
                    storeProgramPointer(branchIp + branch.size);
                } else {
                    cc.set(x86::CondCode::kSignedGT, x);
                    cc.set(x86::CondCode::kSignedLT, y);
                    cc.movzx(x, x.r8());
                    cc.movzx(y, y.r8());
                    cc.sub(x, y);
                    cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), x);
                    storeProgramPointer(ip + in.size);
                }
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

    asPWORD entryId = 1;
    for (size_t i = 0; i < ins.size(); i++) {
        if (ins[i].op == asBC_JitEntry)
            *(asPWORD*)(bc + ins[i].off + 1) = entryId++;
    }

    *out = fn;
    return asSUCCESS;
}

}
