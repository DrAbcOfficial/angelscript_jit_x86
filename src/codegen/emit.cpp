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
    case asBC_CpyRtoV4:
    case asBC_CpyRtoV8:
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
constexpr bool kInlineLocalV4 = true;
constexpr bool kInlineLocalV8 = true;
constexpr bool kInlineValueR4 = true;
constexpr bool kInlineCallV8  = true;
constexpr bool kInlineAdd64   = true;
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
    const bool inlineFieldMemory = ins.size() <= 256;

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
            if ((ins[i].op != asBC_CMPi && ins[i].op != asBC_CMPIi) ||
                !IsConditionalBranch(ins[i + 1].op) || needsLabel[i + 1])
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
            break;
        }
        case asBC_PshC4: {
            if (kInlinePshC4) {
                x86::Gp sp = cc.new_gp32("sp");
                loadSp(sp);
                cc.sub(sp, 4);
                cc.mov(x86::dword_ptr(sp), Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
                storeSp(sp);
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
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_PshV8: {
            if (kInlineCallV8) {
                int source = asBC_SWORDARG0(ip);
                x86::Gp sp = cc.new_gp32("sp");
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                loadSp(sp);
                cc.sub(sp, 8);
                cc.mov(low, x86::dword_ptr(fp, -source * 4));
                cc.mov(high, x86::dword_ptr(fp, -source * 4 + 4));
                cc.mov(x86::dword_ptr(sp), low);
                cc.mov(x86::dword_ptr(sp, 4), high);
                storeSp(sp);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_VAR: {
            x86::Gp sp = cc.new_gp32("sp");
            loadSp(sp);
            cc.sub(sp, 4);
            cc.mov(x86::dword_ptr(sp), Imm(int64_t(asBC_SWORDARG0(ip))));
            storeSp(sp);
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
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_SetV4: {
            if (kInlineLocalV4) {
                int offset = asBC_SWORDARG0(ip);
                cc.mov(x86::dword_ptr(fp, -offset * 4),
                       Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyVtoV4: {
            if (kInlineLocalV4) {
                int destination = asBC_SWORDARG0(ip);
                int source = asBC_SWORDARG1(ip);
                x86::Gp value = cc.new_gp32("value");
                loadVar(source, value);
                storeVar(destination, value);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyVtoR4: {
            if (kInlineValueR4) {
                int source = asBC_SWORDARG0(ip);
                x86::Gp value = cc.new_gp32("value");
                loadVar(source, value);
                cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), value);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyVtoR8: {
            if (kInlineCallV8) {
                int source = asBC_SWORDARG0(ip);
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(fp, -source * 4));
                cc.mov(high, x86::dword_ptr(fp, -source * 4 + 4));
                cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), low);
                cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister) + 4), high);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyRtoV4: {
            if (kInlineValueR4) {
                int destination = asBC_SWORDARG0(ip);
                x86::Gp value = cc.new_gp32("value");
                cc.mov(value, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
                storeVar(destination, value);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyRtoV8: {
            if (kInlineCallV8) {
                int destination = asBC_SWORDARG0(ip);
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
                cc.mov(high, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister) + 4));
                cc.mov(x86::dword_ptr(fp, -destination * 4), low);
                cc.mov(x86::dword_ptr(fp, -destination * 4 + 4), high);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_SetV8: {
            if (kInlineLocalV8) {
                int offset = asBC_SWORDARG0(ip);
                asQWORD value = asBC_QWORDARG(ip);
                cc.mov(x86::dword_ptr(fp, -offset * 4),
                       Imm(int64_t((int32_t)asDWORD(value))));
                cc.mov(x86::dword_ptr(fp, -offset * 4 + 4),
                       Imm(int64_t((int32_t)asDWORD(value >> 32))));
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_STOREOBJ: {
            int destination = asBC_SWORDARG0(ip);
            x86::Gp object = cc.new_gp32("object");
            cc.mov(object, x86::dword_ptr(regs, offsetof(asSVMRegisters, objectRegister)));
            storeVar(destination, object);
            cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, objectRegister)), 0);
            break;
        }
        case asBC_GETOBJ:
        case asBC_GETOBJREF: {
            x86::Gp sp = cc.new_gp32("sp");
            x86::Gp slot = cc.new_gp32("slot");
            x86::Gp offset = cc.new_gp32("offset");
            x86::Gp object = cc.new_gp32("object");
            loadSp(sp);
            cc.lea(slot, x86::dword_ptr(sp, asBC_WORDARG0(ip) * 4));
            cc.mov(offset, x86::dword_ptr(slot));
            cc.shl(offset, 2);
            cc.neg(offset);
            cc.mov(object, x86::dword_ptr(fp, offset));
            cc.mov(x86::dword_ptr(slot), object);
            if (in.op == asBC_GETOBJ)
                cc.mov(x86::dword_ptr(fp, offset), 0);
            break;
        }
        case asBC_CpyVtoV8: {
            if (kInlineLocalV8) {
                int destination = asBC_SWORDARG0(ip);
                int source = asBC_SWORDARG1(ip);
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(fp, -source * 4));
                cc.mov(high, x86::dword_ptr(fp, -source * 4 + 4));
                cc.mov(x86::dword_ptr(fp, -destination * 4), low);
                cc.mov(x86::dword_ptr(fp, -destination * 4 + 4), high);
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CpyVtoG4: {
            int source = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp value = cc.new_gp32("value");
            loadVar(source, value);
            cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
            cc.mov(x86::dword_ptr(address), value);
            break;
        }
        case asBC_CpyGtoV4: {
            int destination = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp value = cc.new_gp32("value");
            cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
            cc.mov(value, x86::dword_ptr(address));
            storeVar(destination, value);
            break;
        }
        case asBC_SetG4: {
            x86::Gp address = cc.new_gp32("address");
            cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
            cc.mov(x86::dword_ptr(address),
                   Imm(int64_t((int32_t)asBC_DWORDARG(ip + AS_PTR_SIZE))));
            break;
        }
        case asBC_WRTV1:
        case asBC_WRTV2:
        case asBC_WRTV4: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            int source = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp value = cc.new_gp32("value");
            cc.mov(address, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
            loadVar(source, value);
            if (in.op == asBC_WRTV1)
                cc.mov(x86::byte_ptr(address), value.r8());
            else if (in.op == asBC_WRTV2)
                cc.mov(x86::word_ptr(address), value.r16());
            else
                cc.mov(x86::dword_ptr(address), value);
            break;
        }
        case asBC_WRTV8: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            int source = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            cc.mov(address, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
            cc.mov(low, x86::dword_ptr(fp, -source * 4));
            cc.mov(high, x86::dword_ptr(fp, -source * 4 + 4));
            cc.mov(x86::dword_ptr(address), low);
            cc.mov(x86::dword_ptr(address, 4), high);
            break;
        }
        case asBC_RDR1:
        case asBC_RDR2:
        case asBC_RDR4: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            int destination = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp value = cc.new_gp32("value");
            cc.mov(address, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
            if (in.op == asBC_RDR1)
                cc.movzx(value, x86::byte_ptr(address));
            else if (in.op == asBC_RDR2)
                cc.movzx(value, x86::word_ptr(address));
            else
                cc.mov(value, x86::dword_ptr(address));
            storeVar(destination, value);
            break;
        }
        case asBC_RDR8: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            int destination = asBC_SWORDARG0(ip);
            x86::Gp address = cc.new_gp32("address");
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            cc.mov(address, x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)));
            cc.mov(low, x86::dword_ptr(address));
            cc.mov(high, x86::dword_ptr(address, 4));
            cc.mov(x86::dword_ptr(fp, -destination * 4), low);
            cc.mov(x86::dword_ptr(fp, -destination * 4 + 4), high);
            break;
        }
        case asBC_LoadVObjR: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            int objectOffset = asBC_SWORDARG0(ip);
            int propertyOffset = asBC_SWORDARG1(ip);
            x86::Gp address = cc.new_gp32("address");
            cc.lea(address, x86::dword_ptr(fp, -objectOffset * 4));
            cc.add(address, propertyOffset);
            cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), address);
            break;
        }
        case asBC_LoadThisR:
        case asBC_LoadRObjR: {
            if (!inlineFieldMemory) {
                if (!emitHelperCall()) return asERROR;
                break;
            }
            const int objectOffset = in.op == asBC_LoadThisR ? 0 : asBC_SWORDARG0(ip);
            const int propertyOffset = in.op == asBC_LoadThisR ?
                                       asBC_SWORDARG0(ip) : asBC_SWORDARG1(ip);
            x86::Gp address = cc.new_gp32("address");
            Label fallback = cc.new_label();
            Label done = cc.new_label();
            cc.mov(address, x86::dword_ptr(fp, -objectOffset * 4));
            cc.test(address, address);
            cc.jz(fallback);
            cc.add(address, propertyOffset);
            cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), address);
            cc.jmp(done);
            cc.bind(fallback);
            if (!emitHelperCall()) return asERROR;
            cc.bind(done);
            break;
        }
        case asBC_ChkNullV: {
            int source = asBC_SWORDARG0(ip);
            Label fallback = cc.new_label();
            Label done = cc.new_label();
            cc.cmp(x86::dword_ptr(fp, -source * 4), 0);
            cc.je(fallback);
            cc.jmp(done);
            cc.bind(fallback);
            if (!emitHelperCall()) return asERROR;
            cc.bind(done);
            break;
        }
        case asBC_ADDi64: {
            if (kInlineAdd64) {
                int destination = asBC_SWORDARG0(ip);
                int left = asBC_SWORDARG1(ip);
                int right = asBC_SWORDARG2(ip);
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                x86::Gp rightLow = cc.new_gp32("rightLow");
                x86::Gp rightHigh = cc.new_gp32("rightHigh");
                cc.mov(low, x86::dword_ptr(fp, -left * 4));
                cc.mov(high, x86::dword_ptr(fp, -left * 4 + 4));
                cc.mov(rightLow, x86::dword_ptr(fp, -right * 4));
                cc.mov(rightHigh, x86::dword_ptr(fp, -right * 4 + 4));
                cc.add(low, rightLow);
                cc.adc(high, rightHigh);
                cc.mov(x86::dword_ptr(fp, -destination * 4), low);
                cc.mov(x86::dword_ptr(fp, -destination * 4 + 4), high);
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
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_ADDf:
        case asBC_SUBf:
        case asBC_MULf: {
            int destination = asBC_SWORDARG0(ip);
            int left = asBC_SWORDARG1(ip);
            int right = asBC_SWORDARG2(ip);
            x86::Vec value = cc.new_xmm_ss("value");
            cc.movss(value, x86::dword_ptr(fp, -left * 4));
            if (in.op == asBC_ADDf)
                cc.addss(value, x86::dword_ptr(fp, -right * 4));
            else if (in.op == asBC_SUBf)
                cc.subss(value, x86::dword_ptr(fp, -right * 4));
            else
                cc.mulss(value, x86::dword_ptr(fp, -right * 4));
            cc.movss(x86::dword_ptr(fp, -destination * 4), value);
            break;
        }
        case asBC_ADDd:
        case asBC_SUBd:
        case asBC_MULd: {
            int destination = asBC_SWORDARG0(ip);
            int left = asBC_SWORDARG1(ip);
            int right = asBC_SWORDARG2(ip);
            x86::Vec value = cc.new_xmm_sd("value");
            cc.movsd(value, x86::qword_ptr(fp, -left * 4));
            if (in.op == asBC_ADDd)
                cc.addsd(value, x86::qword_ptr(fp, -right * 4));
            else if (in.op == asBC_SUBd)
                cc.subsd(value, x86::qword_ptr(fp, -right * 4));
            else
                cc.mulsd(value, x86::qword_ptr(fp, -right * 4));
            cc.movsd(x86::qword_ptr(fp, -destination * 4), value);
            break;
        }
        case asBC_DIVf: {
            int destination = asBC_SWORDARG0(ip);
            int left = asBC_SWORDARG1(ip);
            int right = asBC_SWORDARG2(ip);
            x86::Gp divisorBits = cc.new_gp32("divisorBits");
            Label fallback = cc.new_label();
            Label done = cc.new_label();
            loadVar(right, divisorBits);
            cc.and_(divisorBits, 0x7FFFFFFF);
            cc.jz(fallback);
            {
                x86::Vec value = cc.new_xmm_ss("value");
                cc.movss(value, x86::dword_ptr(fp, -left * 4));
                cc.divss(value, x86::dword_ptr(fp, -right * 4));
                cc.movss(x86::dword_ptr(fp, -destination * 4), value);
            }
            cc.jmp(done);
            cc.bind(fallback);
            if (!emitHelperCall()) return asERROR;
            cc.bind(done);
            break;
        }
        case asBC_DIVd: {
            int destination = asBC_SWORDARG0(ip);
            int left = asBC_SWORDARG1(ip);
            int right = asBC_SWORDARG2(ip);
            x86::Gp zeroTest = cc.new_gp32("zeroTest");
            x86::Gp high = cc.new_gp32("high");
            Label fallback = cc.new_label();
            Label done = cc.new_label();
            cc.mov(zeroTest, x86::dword_ptr(fp, -right * 4));
            cc.mov(high, x86::dword_ptr(fp, -right * 4 + 4));
            cc.and_(high, 0x7FFFFFFF);
            cc.or_(zeroTest, high);
            cc.jz(fallback);
            {
                x86::Vec value = cc.new_xmm_sd("value");
                cc.movsd(value, x86::qword_ptr(fp, -left * 4));
                cc.divsd(value, x86::qword_ptr(fp, -right * 4));
                cc.movsd(x86::qword_ptr(fp, -destination * 4), value);
            }
            cc.jmp(done);
            cc.bind(fallback);
            if (!emitHelperCall()) return asERROR;
            cc.bind(done);
            break;
        }
        case asBC_ADDIf:
        case asBC_SUBIf:
        case asBC_MULIf: {
            int destination = asBC_SWORDARG0(ip);
            int source = asBC_SWORDARG1(ip);
            x86::Gp immediate = cc.new_gp32("immediate");
            x86::Vec value = cc.new_xmm_ss("value");
            x86::Vec operand = cc.new_xmm_ss("operand");
            cc.movss(value, x86::dword_ptr(fp, -source * 4));
            cc.mov(immediate, Imm(int64_t((int32_t)asBC_DWORDARG(ip + 1))));
            cc.movd(operand, immediate);
            if (in.op == asBC_ADDIf)
                cc.addss(value, operand);
            else if (in.op == asBC_SUBIf)
                cc.subss(value, operand);
            else
                cc.mulss(value, operand);
            cc.movss(x86::dword_ptr(fp, -destination * 4), value);
            break;
        }
        case asBC_NEGf: {
            cc.xor_(x86::dword_ptr(fp, -asBC_SWORDARG0(ip) * 4),
                    Imm(int64_t(uint32_t(0x80000000u))));
            break;
        }
        case asBC_NEGd: {
            cc.xor_(x86::dword_ptr(fp, -asBC_SWORDARG0(ip) * 4 + 4),
                    Imm(int64_t(uint32_t(0x80000000u))));
            break;
        }
        case asBC_CMPf:
        case asBC_CMPd:
        case asBC_CMPIf: {
            x86::Vec left = in.op == asBC_CMPd ?
                            cc.new_xmm_sd("left") : cc.new_xmm_ss("left");
            x86::Vec right = in.op == asBC_CMPd ?
                             cc.new_xmm_sd("right") : cc.new_xmm_ss("right");
            if (in.op == asBC_CMPd) {
                cc.movsd(left, x86::qword_ptr(fp, -asBC_SWORDARG0(ip) * 4));
                cc.movsd(right, x86::qword_ptr(fp, -asBC_SWORDARG1(ip) * 4));
                cc.ucomisd(left, right);
            } else {
                cc.movss(left, x86::dword_ptr(fp, -asBC_SWORDARG0(ip) * 4));
                if (in.op == asBC_CMPf) {
                    cc.movss(right, x86::dword_ptr(fp, -asBC_SWORDARG1(ip) * 4));
                } else {
                    x86::Gp immediate = cc.new_gp32("immediate");
                    cc.mov(immediate, Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
                    cc.movd(right, immediate);
                }
                cc.ucomiss(left, right);
            }

            x86::Gp result = cc.new_gp32("result");
            Label less = cc.new_label();
            Label equal = cc.new_label();
            Label done = cc.new_label();
            cc.jp(done);
            cc.jb(less);
            cc.je(equal);
            cc.bind(done);
            cc.mov(result, 1);
            Label store = cc.new_label();
            cc.jmp(store);
            cc.bind(less);
            cc.mov(result, -1);
            cc.jmp(store);
            cc.bind(equal);
            cc.xor_(result, result);
            cc.bind(store);
            cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), result);
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
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_CMPi:
        case asBC_CMPIi: {
            if (kInlineCmp6c) {
                int a0 = asBC_SWORDARG0(ip);
                x86::Gp x = cc.new_gp32("x");
                loadVar(a0, x);
                x86::Gp y = cc.new_gp32("y");
                if (in.op == asBC_CMPi) {
                    loadVar(asBC_SWORDARG1(ip), y);
                    cc.cmp(x, y);
                } else {
                    cc.cmp(x, Imm(int64_t(asBC_INTARG(ip))));
                }
                if (fusedCmpBranch[i]) {
                    const EmitIns& branch = ins[i + 1];
                    const asDWORD* branchIp = bc + branch.off;
                    int64_t target = int64_t(branch.off) + 2 + asBC_INTARG(branchIp);
                    int targetIndex = indexOfOffset[static_cast<size_t>(target)];
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
                } else {
                    cc.set(x86::CondCode::kSignedGT, x);
                    cc.set(x86::CondCode::kSignedLT, y);
                    cc.movzx(x, x.r8());
                    cc.movzx(y, y.r8());
                    cc.sub(x, y);
                    cc.mov(x86::dword_ptr(regs, offsetof(asSVMRegisters, valueRegister)), x);
                }
            } else if (!emitHelperCall()) return asERROR;
            break;
        }
        case asBC_JitEntry:
            break;
        case asBC_SUSPEND: {
            Label process = cc.new_label();
            Label done = cc.new_label();
            cc.cmp(x86::byte_ptr(regs, offsetof(asSVMRegisters, doProcessSuspend)), 0);
            cc.jne(process);
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
