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
            cc.mov(x86::dword_ptr(fp_, -offset * 4),
                   Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
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
            cc.mov(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)),
                   value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoR8: {
        if (kInlineCallV8) {
            const int source = asBC_SWORDARG0(ip);
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            cc.mov(low, x86::dword_ptr(fp_, -source * 4));
            cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
            cc.mov(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)),
                   low);
            cc.mov(x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister) + 4),
                   high);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyRtoV4: {
        if (kInlineValueR4) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("value");
            cc.mov(value,
                   x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)));
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyRtoV8: {
        if (kInlineCallV8) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            cc.mov(low,
                   x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)));
            cc.mov(high,
                   x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister) + 4));
            cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
            cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_SetV8: {
        if (kInlineLocalV8) {
            const int offset = asBC_SWORDARG0(ip);
            const asQWORD value = asBC_QWORDARG(ip);
            cc.mov(x86::dword_ptr(fp_, -offset * 4),
                   Imm(int64_t((int32_t)asDWORD(value))));
            cc.mov(x86::dword_ptr(fp_, -offset * 4 + 4),
                   Imm(int64_t((int32_t)asDWORD(value >> 32))));
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CpyVtoV8: {
        if (kInlineLocalV8) {
            const int destination = asBC_SWORDARG0(ip);
            const int source = asBC_SWORDARG1(ip);
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            cc.mov(low, x86::dword_ptr(fp_, -source * 4));
            cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
            cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
            cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
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
        cc.mov(address,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
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
        cc.mov(address,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
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
        cc.mov(address,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
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
        cc.mov(address,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
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
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               address);
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
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               address);
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
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               address);
        return EmitResult::Success;
    }
    case asBC_INCi:
    case asBC_DECi: {
        x86::Gp address = cc.new_gp32("address");
        cc.mov(address,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
        if (instruction.op == asBC_INCi)
            cc.inc(x86::dword_ptr(address));
        else
            cc.dec(x86::dword_ptr(address));
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
