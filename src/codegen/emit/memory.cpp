#include "codegen/emit/emitter.h"

#include "as_texts.h"

#include <cstddef>

namespace asjitx86::emit {

EmitResult FunctionEmitter::EmitMemory(size_t index,
                                       const Instruction& instruction,
                                       const asDWORD* ip) {
    using namespace asmjit;

    constexpr bool kInlineLocalV4 = true;
    constexpr bool kInlineLocalV8 = true;
    constexpr bool kInlineValueR4 = true;
    constexpr bool kInlineCallV8 = true;
    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_SetV4: {
        if (kInlineLocalV4) {
            const int offset = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("value");
            cc.mov(value, Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
            StoreVar(offset, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoV4: {
        if (kInlineLocalV4) {
            const int destination = asBC_SWORDARG0(ip);
            const int source = asBC_SWORDARG1(ip);
            x86::Gp value = cc.new_gp32("value");
            LoadVar(source, value);
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoR4: {
        if (kInlineValueR4) {
            const int source = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("value");
            LoadVar(source, value);
            StoreValueRegisterForwarded(value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoR8: {
        if (kInlineCallV8) {
            const int source = asBC_SWORDARG0(ip);
            if (cacheLocals_) {
                x86::Vec value = cc.new_xmm("value");
                LoadVar64(source, value);
                cc.movq(x86::qword_ptr(
                            regs_, offsetof(asSVMRegisters, valueRegister)),
                        value);
            } else {
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(fp_, -source * 4));
                cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
                StoreValueRegisterForwarded(low);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister) + 4),
                       high);
            }
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyRtoV4: {
        if (kInlineValueR4) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("value");
            LoadValueRegisterForwarded(value);
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyRtoV8: {
        if (kInlineCallV8) {
            const int destination = asBC_SWORDARG0(ip);
            if (cacheLocals_) {
                x86::Vec value = cc.new_xmm("value");
                cc.movq(value,
                        x86::qword_ptr(
                            regs_, offsetof(asSVMRegisters, valueRegister)));
                StoreVar64(destination, value);
            } else {
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                LoadValueRegisterForwarded(low);
                cc.mov(high,
                       x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister) + 4));
                cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
                cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
            }
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_SetV8: {
        if (kInlineLocalV8) {
            const int offset = asBC_SWORDARG0(ip);
            const asQWORD value = asBC_QWORDARG(ip);
            if (cacheLocals_) {
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, Imm(int64_t((int32_t)asDWORD(value))));
                cc.mov(high,
                       Imm(int64_t((int32_t)asDWORD(value >> 32))));
                StoreVar(offset, low);
                StoreVar(offset - 1, high);
            } else {
                cc.mov(x86::dword_ptr(fp_, -offset * 4),
                       Imm(int64_t((int32_t)asDWORD(value))));
                cc.mov(x86::dword_ptr(fp_, -offset * 4 + 4),
                       Imm(int64_t((int32_t)asDWORD(value >> 32))));
            }
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoV8: {
        if (kInlineLocalV8) {
            const int destination = asBC_SWORDARG0(ip);
            const int source = asBC_SWORDARG1(ip);
            if (cacheLocals_) {
                x86::Vec value = cc.new_xmm("value");
                LoadVar64(source, value);
                StoreVar64(destination, value);
            } else {
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(fp_, -source * 4));
                cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
                cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
                cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
            }
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoG4: {
        const int source = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp value = cc.new_gp32("value");
        LoadVar(source, value);
        cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        cc.mov(x86::dword_ptr(address), value);
        return EmitResult::Success;
    }
    case asBC_CpyGtoV4: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp value = cc.new_gp32("value");
        cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        cc.mov(value, x86::dword_ptr(address));
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_SetG4: {
        x86::Gp address = cc.new_gp32("address");
        cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        cc.mov(x86::dword_ptr(address),
               Imm(int64_t((int32_t)asBC_DWORDARG(ip + AS_PTR_SIZE))));
        return EmitResult::Success;
    }
    case asBC_WRTV1:
    case asBC_WRTV2:
    case asBC_WRTV4: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int source = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp value = cc.new_gp32("value");
        LoadValueRegisterForwarded(address);
        LoadVar(source, value);
        if (instruction.op == asBC_WRTV1)
            cc.mov(x86::byte_ptr(address), value.r8());
        else if (instruction.op == asBC_WRTV2)
            cc.mov(x86::word_ptr(address), value.r16());
        else
            cc.mov(x86::dword_ptr(address), value);
        return EmitResult::Success;
    }
    case asBC_WRTV8: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int source = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadValueRegisterForwarded(address);
        cc.mov(low, x86::dword_ptr(fp_, -source * 4));
        cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
        cc.mov(x86::dword_ptr(address), low);
        cc.mov(x86::dword_ptr(address, 4), high);
        return EmitResult::Success;
    }
    case asBC_RDR1:
    case asBC_RDR2:
    case asBC_RDR4: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp value = cc.new_gp32("value");
        LoadValueRegisterForwarded(address);
        if (instruction.op == asBC_RDR1)
            cc.movzx(value, x86::byte_ptr(address));
        else if (instruction.op == asBC_RDR2)
            cc.movzx(value, x86::word_ptr(address));
        else
            cc.mov(value, x86::dword_ptr(address));
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_RDR8: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadValueRegisterForwarded(address);
        cc.mov(low, x86::dword_ptr(address));
        cc.mov(high, x86::dword_ptr(address, 4));
        cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
        cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
        return EmitResult::Success;
    }
    case asBC_LoadVObjR: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int objectOffset = asBC_SWORDARG0(ip);
        const int propertyOffset = asBC_SWORDARG1(ip);
        x86::Gp address = cc.new_gp32("address");
        cc.lea(address,
               x86::dword_ptr(fp_, -objectOffset * 4));
        cc.add(address, propertyOffset);
        StoreValueRegisterForwarded(address);
        return EmitResult::Success;
    }
    case asBC_LoadThisR:
    case asBC_LoadRObjR: {
        if (!inlineFieldMemory_) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        const int objectOffset =
            instruction.op == asBC_LoadThisR ? 0 : asBC_SWORDARG0(ip);
        const int propertyOffset = instruction.op == asBC_LoadThisR
                                       ? asBC_SWORDARG0(ip)
                                       : asBC_SWORDARG1(ip);
        x86::Gp address = cc.new_gp32("address");
        Label fallback = cc.new_label();
        Label done = cc.new_label();
        cc.mov(address, x86::dword_ptr(fp_, -objectOffset * 4));
        cc.test(address, address);
        cc.jz(fallback);
        cc.add(address, propertyOffset);
        StoreValueRegisterForwarded(address);
        cc.jmp(done);
        cc.bind(fallback);
        if (!EmitInternalException(index, ip, TXT_NULL_POINTER_ACCESS))
            return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_ChkNullV: {
        const int source = asBC_SWORDARG0(ip);
        Label fallback = cc.new_label();
        Label done = cc.new_label();
        cc.cmp(x86::dword_ptr(fp_, -source * 4), 0);
        cc.je(fallback);
        cc.jmp(done);
        cc.bind(fallback);
        if (!EmitInternalException(index, ip, TXT_NULL_POINTER_ACCESS))
            return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_LDG:
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        return EmitResult::Success;
    case asBC_LDV: {
        const int source = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        cc.lea(address, x86::dword_ptr(fp_, -source * 4));
        StoreValueRegisterForwarded(address);
        return EmitResult::Success;
    }
    case asBC_INCi:
    case asBC_DECi: {
        x86::Gp address = cc.new_gp32("address");
        LoadValueRegisterForwarded(address);
        if (instruction.op == asBC_INCi)
            cc.inc(x86::dword_ptr(address));
        else
            cc.dec(x86::dword_ptr(address));
        return EmitResult::Success;
    }
    case asBC_INCi8:
    case asBC_DECi8: {
        x86::Gp address = cc.new_gp32("address");
        LoadValueRegisterForwarded(address);
        if (instruction.op == asBC_INCi8)
            cc.inc(x86::byte_ptr(address));
        else
            cc.dec(x86::byte_ptr(address));
        return EmitResult::Success;
    }
    case asBC_INCi16:
    case asBC_DECi16: {
        x86::Gp address = cc.new_gp32("address");
        LoadValueRegisterForwarded(address);
        if (instruction.op == asBC_INCi16)
            cc.inc(x86::word_ptr(address));
        else
            cc.dec(x86::word_ptr(address));
        return EmitResult::Success;
    }
    case asBC_INCf:
    case asBC_DECf: {
        x86::Gp address = cc.new_gp32("address");
        x86::Vec value = cc.new_xmm_ss("incValue");
        x86::Vec one = cc.new_xmm_ss("incOne");
        LoadValueRegisterForwarded(address);
        const x86::Mem oneMem = cc.new_float_const(ConstPoolScope::kLocal, 1.0f);
        if (useAvx_) {
            cc.vmovss(value, x86::dword_ptr(address));
            cc.vmovss(one, oneMem);
            if (instruction.op == asBC_INCf)
                cc.vaddss(value, value, one);
            else
                cc.vsubss(value, value, one);
            cc.vmovss(x86::dword_ptr(address), value);
        } else {
            cc.movss(value, x86::dword_ptr(address));
            cc.movss(one, oneMem);
            if (instruction.op == asBC_INCf)
                cc.addss(value, one);
            else
                cc.subss(value, one);
            cc.movss(x86::dword_ptr(address), value);
        }
        return EmitResult::Success;
    }
    case asBC_INCd:
    case asBC_DECd: {
        x86::Gp address = cc.new_gp32("address");
        x86::Vec value = cc.new_xmm_sd("incValue");
        x86::Vec one = cc.new_xmm_sd("incOne");
        LoadValueRegisterForwarded(address);
        const x86::Mem oneMem = cc.new_double_const(ConstPoolScope::kLocal, 1.0);
        if (useAvx_) {
            cc.vmovsd(value, x86::qword_ptr(address));
            cc.vmovsd(one, oneMem);
            if (instruction.op == asBC_INCd)
                cc.vaddsd(value, value, one);
            else
                cc.vsubsd(value, value, one);
            cc.vmovsd(x86::qword_ptr(address), value);
        } else {
            cc.movsd(value, x86::qword_ptr(address));
            cc.movsd(one, oneMem);
            if (instruction.op == asBC_INCd)
                cc.addsd(value, one);
            else
                cc.subsd(value, one);
            cc.movsd(x86::qword_ptr(address), value);
        }
        return EmitResult::Success;
    }
    case asBC_INCi64:
    case asBC_DECi64: {
        x86::Gp address = cc.new_gp32("address");
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadValueRegisterForwarded(address);
        cc.mov(low, x86::dword_ptr(address));
        cc.mov(high, x86::dword_ptr(address, 4));
        if (instruction.op == asBC_INCi64) {
            cc.add(low, 1);
            cc.adc(high, 0);
        } else {
            cc.sub(low, 1);
            cc.sbb(high, 0);
        }
        cc.mov(x86::dword_ptr(address), low);
        cc.mov(x86::dword_ptr(address, 4), high);
        return EmitResult::Success;
    }
    case asBC_ClrVPtr: {
        x86::Gp zero = cc.new_gp32("zero");
        cc.xor_(zero, zero);
        StoreVar(asBC_SWORDARG0(ip), zero);
        return EmitResult::Success;
    }
    case asBC_LdGRdR4: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp address = cc.new_gp32("address");
        x86::Gp value = cc.new_gp32("value");
        cc.mov(address, Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        StoreValueRegisterForwarded(address);
        cc.mov(value, x86::dword_ptr(address));
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_sbTOi:
    case asBC_ubTOi:
    case asBC_swTOi:
    case asBC_uwTOi:
    case asBC_iTOb:
    case asBC_iTOw: {
        const int offset = asBC_SWORDARG0(ip);
        x86::Gp value = cc.new_gp32("value");
        LoadVar(offset, value);
        if (instruction.op == asBC_sbTOi)
            cc.movsx(value, value.r8());
        else if (instruction.op == asBC_ubTOi)
            cc.movzx(value, value.r8());
        else if (instruction.op == asBC_swTOi)
            cc.movsx(value, value.r16());
        else if (instruction.op == asBC_uwTOi)
            cc.movzx(value, value.r16());
        else if (instruction.op == asBC_iTOb)
            cc.movzx(value, value.r8());
        else
            cc.movzx(value, value.r16());
        StoreVar(offset, value);
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
