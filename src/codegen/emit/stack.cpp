#include "codegen/emit/emitter.h"

namespace asjitx86::emit {

EmitResult FunctionEmitter::EmitStack(size_t, const Instruction& instruction,
                                      const asDWORD* ip) {
    using namespace asmjit;

    constexpr bool kInlinePshC4 = true;
    constexpr bool kInlinePshV4 = true;
    constexpr bool kInlinePsf = true;
    constexpr bool kInlineCallV8 = true;
    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_PshC4: {
        if (kInlinePshC4) {
            x86::Gp sp = cc.new_gp32("sp");
            LoadSp(sp);
            cc.sub(sp, 4);
            cc.mov(x86::dword_ptr(sp),
                   Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
            StoreSp(sp);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_PshV4: {
        if (kInlinePshV4) {
            const int offset = asBC_SWORDARG0(ip);
            x86::Gp sp = cc.new_gp32("sp");
            x86::Gp value = cc.new_gp32("v");
            LoadSp(sp);
            cc.sub(sp, 4);
            LoadVar(offset, value);
            cc.mov(x86::dword_ptr(sp), value);
            StoreSp(sp);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_PshV8: {
        if (kInlineCallV8) {
            const int source = asBC_SWORDARG0(ip);
            x86::Gp sp = cc.new_gp32("sp");
            LoadSp(sp);
            cc.sub(sp, 8);
            if (useSse_) {
                x86::Vec value = cc.new_xmm("value64");
                LoadVar64(source, value);
                if (useAvx_)
                    cc.vmovq(x86::qword_ptr(sp), value);
                else
                    cc.movq(x86::qword_ptr(sp), value);
            } else {
                x86::Gp low = cc.new_gp32("low");
                x86::Gp high = cc.new_gp32("high");
                cc.mov(low, x86::dword_ptr(fp_, -source * 4));
                cc.mov(high, x86::dword_ptr(fp_, -source * 4 + 4));
                cc.mov(x86::dword_ptr(sp), low);
                cc.mov(x86::dword_ptr(sp, 4), high);
            }
            StoreSp(sp);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_PopPtr: {
        x86::Gp sp = cc.new_gp32("sp");
        LoadSp(sp);
        cc.add(sp, AS_PTR_SIZE * 4);
        StoreSp(sp);
        return EmitResult::Success;
    }
    case asBC_PopRPtr: {
        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp value = cc.new_gp32("value");
        LoadSp(sp);
        cc.mov(value, x86::dword_ptr(sp));
        cc.add(sp, AS_PTR_SIZE * 4);
        StoreSp(sp);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               value);
        return EmitResult::Success;
    }
    case asBC_PshRPtr: {
        x86::Gp sp = cc.new_gp32("sp");
        x86::Gp value = cc.new_gp32("value");
        LoadSp(sp);
        cc.sub(sp, AS_PTR_SIZE * 4);
        cc.mov(value,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
        cc.mov(x86::dword_ptr(sp), value);
        StoreSp(sp);
        return EmitResult::Success;
    }
    case asBC_PGA: {
        x86::Gp sp = cc.new_gp32("sp");
        LoadSp(sp);
        cc.sub(sp, AS_PTR_SIZE * 4);
        cc.mov(x86::dword_ptr(sp),
               Imm(int64_t((intptr_t)asBC_PTRARG(ip))));
        StoreSp(sp);
        return EmitResult::Success;
    }
    case asBC_TYPEID: {
        x86::Gp sp = cc.new_gp32("sp");
        LoadSp(sp);
        cc.sub(sp, 4);
        cc.mov(x86::dword_ptr(sp),
               Imm(int64_t(static_cast<int32_t>(asBC_DWORDARG(ip)))));
        StoreSp(sp);
        return EmitResult::Success;
    }
    case asBC_VAR: {
        x86::Gp sp = cc.new_gp32("sp");
        LoadSp(sp);
        cc.sub(sp, 4);
        cc.mov(x86::dword_ptr(sp), Imm(int64_t(asBC_SWORDARG0(ip))));
        StoreSp(sp);
        return EmitResult::Success;
    }
    case asBC_PSF: {
        if (kInlinePsf) {
            const int offset = asBC_SWORDARG0(ip);
            x86::Gp sp = cc.new_gp32("sp");
            x86::Gp value = cc.new_gp32("v");
            LoadSp(sp);
            cc.sub(sp, 4);
            cc.lea(value, x86::dword_ptr(fp_, -offset * 4));
            cc.mov(x86::dword_ptr(sp), value);
            StoreSp(sp);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
