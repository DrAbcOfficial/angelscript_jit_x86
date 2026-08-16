#include "codegen/emit/emitter.h"

#include "as_texts.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace asjitx86::emit {

size_t FunctionEmitter::EmitPackedFloatImmediate(size_t index) {
    using namespace asmjit;

    if (!useSse_ || cacheLocals_ || index >= instructions_.size()) return 0;
    const asEBCInstr op = instructions_[index].op;
    if (op != asBC_ADDIf && op != asBC_SUBIf && op != asBC_MULIf) return 0;

    auto tryWidth = [&](size_t width, size_t stride) -> size_t {
        const size_t span = (width - 1) * stride + 1;
        if (index + span > instructions_.size()) return 0;
        std::array<uint32_t, 8> constants{};
        int firstOffset = 0;
        int step = 0;
        asDWORD immediate = 0;
        for (size_t lane = 0; lane < width; lane++) {
            const size_t current = index + lane * stride;
            if (lane && stride == 2) {
                const size_t entry = current - 1;
                if (instructions_[entry].op != asBC_JitEntry ||
                    !needsLabel_[entry])
                    return 0;
            }
            if (instructions_[current].op != op ||
                (lane && needsLabel_[current]) ||
                refCopyFusionSkip_[current] || fusedCmpBranch_[current])
                return 0;
            const asDWORD* currentIp =
                bytecode_ + instructions_[current].off;
            const int destination = asBC_SWORDARG0(currentIp);
            const int source = asBC_SWORDARG1(currentIp);
            if (destination != source) return 0;
            if (!lane) {
                firstOffset = destination;
                immediate = asBC_DWORDARG(currentIp + 1);
            } else {
                if (asBC_DWORDARG(currentIp + 1) != immediate) return 0;
                const int currentStep = destination - firstOffset;
                if (lane == 1) {
                    if (currentStep != 1 && currentStep != -1) return 0;
                    step = currentStep;
                } else if (currentStep != step * static_cast<int>(lane)) {
                    return 0;
                }
            }
            constants[lane] = immediate;
        }

        const int lastOffset = firstOffset +
            step * static_cast<int>(width - 1);
        const int highestOffset = std::max(firstOffset, lastOffset);
        auto& cc = Compiler();
        const x86::Mem valuesMemory = width == 8
            ? x86::ymmword_ptr(fp_, -highestOffset * 4)
            : x86::xmmword_ptr(fp_, -highestOffset * 4);
        const x86::Mem constantMemory = cc.new_const(
            ConstPoolScope::kLocal, constants.data(), width * sizeof(uint32_t));
        x86::Vec values = width == 8
            ? cc.new_ymm_ps("packedFloatValues")
            : cc.new_xmm_ps("packedFloatValues");

        if (width == 8) {
            cc.vmovups(values, valuesMemory);
            if (op == asBC_ADDIf)
                cc.vaddps(values, values, constantMemory);
            else if (op == asBC_SUBIf)
                cc.vsubps(values, values, constantMemory);
            else
                cc.vmulps(values, values, constantMemory);
            cc.vmovups(valuesMemory, values);
        } else {
            cc.movups(values, valuesMemory);
            if (op == asBC_ADDIf)
                cc.addps(values, constantMemory);
            else if (op == asBC_SUBIf)
                cc.subps(values, constantMemory);
            else
                cc.mulps(values, constantMemory);
            cc.movups(valuesMemory, values);
        }

        if (stride == 2) {
            Label packedDone = cc.new_label();
            cc.jmp(packedDone);
            for (size_t lane = 1; lane < width; lane++) {
                const size_t current = index + lane * stride;
                cc.bind(labels_[current - 1]);
                const asDWORD* currentIp =
                    bytecode_ + instructions_[current].off;
                EmitNumeric(current, instructions_[current], currentIp);
            }
            cc.bind(packedDone);
        }
        return span;
    };

    if (useAvx_) {
        size_t avxWidth = tryWidth(8, 2);
        if (avxWidth) return avxWidth;
        avxWidth = tryWidth(8, 1);
        if (avxWidth) return avxWidth;
    }
    size_t sseWidth = tryWidth(4, 2);
    if (sseWidth) return sseWidth;
    return tryWidth(4, 1);
}

EmitResult FunctionEmitter::EmitNumeric(size_t index,
                                        const Instruction& instruction,
                                        const asDWORD* ip) {
    using namespace asmjit;

    constexpr bool kInlineAdd64 = true;
    constexpr bool kInlineAdd6b = true;
    constexpr bool kInlineSub6b = true;
    constexpr bool kInlineMul6b = true;
    constexpr bool kInlineDiv6b = true;
    constexpr bool kInlineBits6b = true;
    constexpr bool kInlineNeg6b = true;
    constexpr bool kInlineNot6b = true;
    constexpr bool kInlineIncDecV = true;
    constexpr bool kInlineImmInt = true;
    constexpr bool kInlineCmp6c = true;
    auto& cc = Compiler();
    switch (instruction.op) {
    case asBC_ADDi64: {
        if (kInlineAdd64) {
            const int destination = asBC_SWORDARG0(ip);
            const int left = asBC_SWORDARG1(ip);
            const int right = asBC_SWORDARG2(ip);
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            x86::Gp rightLow = cc.new_gp32("rightLow");
            x86::Gp rightHigh = cc.new_gp32("rightHigh");
            cc.mov(low, x86::dword_ptr(fp_, -left * 4));
            cc.mov(high, x86::dword_ptr(fp_, -left * 4 + 4));
            cc.mov(rightLow, x86::dword_ptr(fp_, -right * 4));
            cc.mov(rightHigh, x86::dword_ptr(fp_, -right * 4 + 4));
            cc.add(low, rightLow);
            cc.adc(high, rightHigh);
            cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
            cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_iTOi64: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadVar(source, low);
        cc.mov(high, low);
        cc.sar(high, 31);
        cc.mov(x86::dword_ptr(fp_, -destination * 4), low);
        cc.mov(x86::dword_ptr(fp_, -destination * 4 + 4), high);
        return EmitResult::Success;
    }
    case asBC_i64TOi: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Gp value = cc.new_gp32("value");
        LoadVar(source, value);
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_iTOd: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Gp sourceValue = cc.new_gp32("sourceValue");
        x86::Vec value = cc.new_xmm_sd("value");
        LoadVar(source, sourceValue);
        cc.cvtsi2sd(value, sourceValue);
        StoreVar64(destination, value);
        return EmitResult::Success;
    }
    case asBC_dTOi: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Gp value = cc.new_gp32("value");
        x86::Vec sourceValue = cc.new_xmm_sd("sourceValue");
        LoadVar64(source, sourceValue);
        cc.cvttsd2si(value, sourceValue);
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_ADDi:
    case asBC_SUBi:
    case asBC_MULi: {
        const bool shouldInline =
            (instruction.op == asBC_ADDi && kInlineAdd6b) ||
            (instruction.op == asBC_SUBi && kInlineSub6b) ||
            (instruction.op == asBC_MULi && kInlineMul6b);
        if (shouldInline) {
            const int destination = asBC_SWORDARG0(ip);
            const int left = asBC_SWORDARG1(ip);
            const int right = asBC_SWORDARG2(ip);
            x86::Gp x = cc.new_gp32("x");
            x86::Gp y = cc.new_gp32("y");
            LoadVar(left, x);
            LoadVar(right, y);
            switch (instruction.op) {
            case asBC_ADDi:
                cc.add(x, y);
                break;
            case asBC_SUBi:
                cc.sub(x, y);
                break;
            default:
                cc.imul(x, y);
                break;
            }
            StoreVar(destination, x);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_ADDf:
    case asBC_SUBf:
    case asBC_MULf: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Vec value = cc.new_xmm_ss("value");
        LoadFloatVar(left, value);
        if (useSse_ && !cacheLocals_) {
            const x86::Mem operand = x86::dword_ptr(fp_, -right * 4);
            if (instruction.op == asBC_ADDf) {
                cc.addss(value, operand);
            } else if (instruction.op == asBC_SUBf) {
                cc.subss(value, operand);
            } else {
                cc.mulss(value, operand);
            }
        } else {
            x86::Vec operand = cc.new_xmm_ss("operand");
            LoadFloatVar(right, operand);
            if (useAvx_) {
                if (instruction.op == asBC_ADDf)
                    cc.vaddss(value, value, operand);
                else if (instruction.op == asBC_SUBf)
                    cc.vsubss(value, value, operand);
                else
                    cc.vmulss(value, value, operand);
            } else if (instruction.op == asBC_ADDf) {
                cc.addss(value, operand);
            } else if (instruction.op == asBC_SUBf) {
                cc.subss(value, operand);
            } else {
                cc.mulss(value, operand);
            }
        }
        StoreFloatVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_ADDd:
    case asBC_SUBd:
    case asBC_MULd: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Vec value = cc.new_xmm_sd("value");
        LoadDoubleVar(left, value);
        if (useSse_ && !cacheLocals_) {
            const x86::Mem operand = x86::qword_ptr(fp_, -right * 4);
            if (instruction.op == asBC_ADDd) {
                cc.addsd(value, operand);
            } else if (instruction.op == asBC_SUBd) {
                cc.subsd(value, operand);
            } else {
                cc.mulsd(value, operand);
            }
        } else {
            x86::Vec operand = cc.new_xmm_sd("operand");
            LoadDoubleVar(right, operand);
            if (useAvx_) {
                if (instruction.op == asBC_ADDd)
                    cc.vaddsd(value, value, operand);
                else if (instruction.op == asBC_SUBd)
                    cc.vsubsd(value, value, operand);
                else
                    cc.vmulsd(value, value, operand);
            } else if (instruction.op == asBC_ADDd) {
                cc.addsd(value, operand);
            } else if (instruction.op == asBC_SUBd) {
                cc.subsd(value, operand);
            } else {
                cc.mulsd(value, operand);
            }
        }
        StoreDoubleVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_DIVf: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp divisorBits = cc.new_gp32("divisorBits");
        Label fallback = cc.new_label();
        Label done = cc.new_label();
        LoadVar(right, divisorBits);
        cc.and_(divisorBits, 0x7FFFFFFF);
        cc.jz(fallback);
        {
            x86::Vec value = cc.new_xmm_ss("value");
            LoadFloatVar(left, value);
            if (useSse_ && !cacheLocals_) {
                const x86::Mem operand = x86::dword_ptr(fp_, -right * 4);
                cc.divss(value, operand);
            } else {
                x86::Vec operand = cc.new_xmm_ss("operand");
                LoadFloatVar(right, operand);
                if (useAvx_)
                    cc.vdivss(value, value, operand);
                else
                    cc.divss(value, operand);
            }
            StoreFloatVar(destination, value);
        }
        cc.jmp(done);
        cc.bind(fallback);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_DIVd: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp zeroTest = cc.new_gp32("zeroTest");
        x86::Gp high = cc.new_gp32("high");
        Label fallback = cc.new_label();
        Label done = cc.new_label();
        LoadVar(right, zeroTest);
        LoadVar(right - 1, high);
        cc.and_(high, 0x7FFFFFFF);
        cc.or_(zeroTest, high);
        cc.jz(fallback);
        {
            x86::Vec value = cc.new_xmm_sd("value");
            LoadDoubleVar(left, value);
            if (useSse_ && !cacheLocals_) {
                const x86::Mem operand = x86::qword_ptr(fp_, -right * 4);
                cc.divsd(value, operand);
            } else {
                x86::Vec operand = cc.new_xmm_sd("operand");
                LoadDoubleVar(right, operand);
                if (useAvx_)
                    cc.vdivsd(value, value, operand);
                else
                    cc.divsd(value, operand);
            }
            StoreDoubleVar(destination, value);
        }
        cc.jmp(done);
        cc.bind(fallback);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_ADDIf:
    case asBC_SUBIf:
    case asBC_MULIf: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Vec value = cc.new_xmm_ss("value");
        LoadFloatVar(source, value);
        if (useSse_) {
            const x86::Mem operand = cc.new_uint32_const(
                ConstPoolScope::kLocal, asBC_DWORDARG(ip + 1));
            if (useAvx_) {
                if (instruction.op == asBC_ADDIf)
                    cc.vaddss(value, value, operand);
                else if (instruction.op == asBC_SUBIf)
                    cc.vsubss(value, value, operand);
                else
                    cc.vmulss(value, value, operand);
            } else if (instruction.op == asBC_ADDIf) {
                cc.addss(value, operand);
            } else if (instruction.op == asBC_SUBIf) {
                cc.subss(value, operand);
            } else {
                cc.mulss(value, operand);
            }
        } else {
            x86::Gp immediate = cc.new_gp32("immediate");
            x86::Vec operand = cc.new_xmm_ss("operand");
            cc.mov(immediate,
                   Imm(int64_t((int32_t)asBC_DWORDARG(ip + 1))));
            cc.movd(operand, immediate);
            if (instruction.op == asBC_ADDIf)
                cc.addss(value, operand);
            else if (instruction.op == asBC_SUBIf)
                cc.subss(value, operand);
            else
                cc.mulss(value, operand);
        }
        StoreFloatVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_NEGf: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp value = cc.new_gp32("value");
        LoadVar(destination, value);
        cc.xor_(value, Imm(int64_t(uint32_t(0x80000000u))));
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_NEGd: {
        const int highOffset = asBC_SWORDARG0(ip) - 1;
        x86::Gp value = cc.new_gp32("value");
        LoadVar(highOffset, value);
        cc.xor_(value, Imm(int64_t(uint32_t(0x80000000u))));
        StoreVar(highOffset, value);
        return EmitResult::Success;
    }
    case asBC_CMPf:
    case asBC_CMPd:
    case asBC_CMPIf: {
        x86::Vec left = instruction.op == asBC_CMPd
                            ? cc.new_xmm_sd("left")
                            : cc.new_xmm_ss("left");
        x86::Vec right = instruction.op == asBC_CMPd
                             ? cc.new_xmm_sd("right")
                             : cc.new_xmm_ss("right");
        if (instruction.op == asBC_CMPd) {
            LoadDoubleVar(asBC_SWORDARG0(ip), left);
            LoadDoubleVar(asBC_SWORDARG1(ip), right);
            if (useAvx_)
                cc.vucomisd(left, right);
            else
                cc.ucomisd(left, right);
        } else {
            LoadFloatVar(asBC_SWORDARG0(ip), left);
            if (instruction.op == asBC_CMPf) {
                LoadFloatVar(asBC_SWORDARG1(ip), right);
            } else {
                x86::Gp immediate = cc.new_gp32("immediate");
                cc.mov(immediate,
                       Imm(int64_t((int32_t)asBC_DWORDARG(ip))));
                if (useAvx_)
                    cc.vmovd(right, immediate);
                else
                    cc.movd(right, immediate);
            }
            if (useAvx_)
                cc.vucomiss(left, right);
            else
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
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               result);
        return EmitResult::Success;
    }
    case asBC_DIVi:
    case asBC_MODi: {
        if (kInlineDiv6b) {
            const int destination = asBC_SWORDARG0(ip);
            const int left = asBC_SWORDARG1(ip);
            const int right = asBC_SWORDARG2(ip);
            x86::Gp dividend = cc.new_gp32("dividend");
            x86::Gp divisor = cc.new_gp32("divisor");
            x86::Gp high = cc.new_gp32("high");
            Label overflowCheck = cc.new_label();
            Label divideByZero = cc.new_label();
            Label divideOverflow = cc.new_label();
            Label divide = cc.new_label();
            Label done = cc.new_label();
            LoadVar(left, dividend);
            LoadVar(right, divisor);
            cc.test(divisor, divisor);
            cc.jz(divideByZero);
            cc.cmp(divisor, -1);
            cc.je(overflowCheck);
            cc.jmp(divide);
            cc.bind(overflowCheck);
            cc.cmp(dividend, Imm(int64_t(INT32_MIN)));
            cc.je(divideOverflow);
            cc.bind(divide);
            cc.mov(high, dividend);
            cc.sar(high, 31);
            cc.idiv(high, dividend, divisor);
            StoreVar(destination,
                     instruction.op == asBC_DIVi ? dividend : high);
            cc.jmp(done);

            auto emitDivideException = [&](const Label& label,
                                           const char* message) -> bool {
                cc.bind(label);
                return EmitInternalException(index, ip, message);
            };
            if (!emitDivideException(divideByZero, TXT_DIVIDE_BY_ZERO) ||
                !emitDivideException(divideOverflow, TXT_DIVIDE_OVERFLOW))
                return EmitResult::Error;
            cc.bind(done);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_BAND:
    case asBC_BOR:
    case asBC_BXOR:
    case asBC_BSLL:
    case asBC_BSRL:
    case asBC_BSRA: {
        if (kInlineBits6b) {
            const int destination = asBC_SWORDARG0(ip);
            const int left = asBC_SWORDARG1(ip);
            const int right = asBC_SWORDARG2(ip);
            x86::Gp x = cc.new_gp32("x");
            x86::Gp y = cc.new_gp32("y");
            LoadVar(left, x);
            LoadVar(right, y);
            switch (instruction.op) {
            case asBC_BAND:
                cc.and_(x, y);
                break;
            case asBC_BOR:
                cc.or_(x, y);
                break;
            case asBC_BXOR:
                cc.xor_(x, y);
                break;
            case asBC_BSLL:
                cc.shl(x, y);
                break;
            case asBC_BSRL:
                cc.shr(x, y);
                break;
            default:
                cc.sar(x, y);
                break;
            }
            StoreVar(destination, x);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_NEGi: {
        if (kInlineNeg6b) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("x");
            LoadVar(destination, value);
            cc.neg(value);
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_NOT: {
        if (kInlineNot6b) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("x");
            cc.movzx(value,
                     x86::byte_ptr(fp_, -destination * 4));
            cc.test(value, value);
            cc.set(x86::CondCode::kEqual, value);
            cc.movzx(value, value.r8());
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_IncVi:
    case asBC_DecVi: {
        if (kInlineIncDecV) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("value");
            LoadVar(destination, value);
            if (instruction.op == asBC_IncVi)
                cc.inc(value);
            else
                cc.dec(value);
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_ADDIi:
    case asBC_SUBIi:
    case asBC_MULIi: {
        if (kInlineImmInt) {
            const int destination = asBC_SWORDARG0(ip);
            const int source = asBC_SWORDARG1(ip);
            const int32_t immediate = asBC_INTARG(ip + 1);
            x86::Gp value = cc.new_gp32("x");
            LoadVar(source, value);
            switch (instruction.op) {
            case asBC_ADDIi:
                cc.add(value, immediate);
                break;
            case asBC_SUBIi:
                cc.sub(value, immediate);
                break;
            default:
                cc.imul(value, value, immediate);
                break;
            }
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_CMPi:
    case asBC_CMPIi:
    case asBC_CmpPtr: {
        if (kInlineCmp6c) {
            const int left = asBC_SWORDARG0(ip);
            x86::Gp x = cc.new_gp32("x");
            LoadVar(left, x);
            x86::Gp y = cc.new_gp32("y");
            if (instruction.op == asBC_CMPi ||
                instruction.op == asBC_CmpPtr) {
                LoadVar(asBC_SWORDARG1(ip), y);
                cc.cmp(x, y);
            } else {
                cc.cmp(x, Imm(int64_t(asBC_INTARG(ip))));
            }
            if (fusedCmpBranch_[index]) {
                const Instruction& branch = instructions_[index + 1];
                const int targetIndex = BranchTargetIndex(
                    branch, bytecode_ + branch.off);
                if (targetIndex < 0) return EmitResult::Error;
                switch (branch.op) {
                case asBC_JZ:
                    cc.jz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNZ:
                    cc.jnz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JS:
                    if (instruction.op == asBC_CmpPtr)
                        cc.jb(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.js(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNS:
                    if (instruction.op == asBC_CmpPtr)
                        cc.jae(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.jns(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JP:
                    if (instruction.op == asBC_CmpPtr)
                        cc.ja(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.jg(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNP:
                    if (instruction.op == asBC_CmpPtr)
                        cc.jbe(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.jle(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JLowZ:
                    cc.jz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JLowNZ:
                    cc.jnz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                default:
                    return EmitResult::Error;
                }
                if (fusedFallValue_[index] != 2)
                    cc.mov(x86::dword_ptr(
                               regs_, offsetof(asSVMRegisters, valueRegister)),
                           fusedFallValue_[index]);
            } else {
                cc.set(instruction.op == asBC_CmpPtr
                           ? x86::CondCode::kUnsignedGT
                           : x86::CondCode::kSignedGT,
                       x);
                cc.set(instruction.op == asBC_CmpPtr
                           ? x86::CondCode::kUnsignedLT
                           : x86::CondCode::kSignedLT,
                       y);
                cc.movzx(x, x.r8());
                cc.movzx(y, y.r8());
                cc.sub(x, y);
                cc.mov(x86::dword_ptr(
                           regs_, offsetof(asSVMRegisters, valueRegister)),
                       x);
            }
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
