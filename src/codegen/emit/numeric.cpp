#include "codegen/emit/emitter.h"

#include "as_texts.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace asjitx86::emit {

namespace {

void DivI64To(asDWORD loA, asDWORD hiA, asDWORD loB, asDWORD hiB,
              asDWORD* out) {
    const asQWORD a = asQWORD(loA) | (asQWORD(hiA) << 32);
    const asQWORD b = asQWORD(loB) | (asQWORD(hiB) << 32);
    const asQWORD result = asQWORD(asINT64(a) / asINT64(b));
    out[0] = asDWORD(result);
    out[1] = asDWORD(result >> 32);
}

void ModI64To(asDWORD loA, asDWORD hiA, asDWORD loB, asDWORD hiB,
              asDWORD* out) {
    const asQWORD a = asQWORD(loA) | (asQWORD(hiA) << 32);
    const asQWORD b = asQWORD(loB) | (asQWORD(hiB) << 32);
    const asQWORD result = asQWORD(asINT64(a) % asINT64(b));
    out[0] = asDWORD(result);
    out[1] = asDWORD(result >> 32);
}

void DivU64To(asDWORD loA, asDWORD hiA, asDWORD loB, asDWORD hiB,
              asDWORD* out) {
    const asQWORD a = asQWORD(loA) | (asQWORD(hiA) << 32);
    const asQWORD b = asQWORD(loB) | (asQWORD(hiB) << 32);
    const asQWORD result = a / b;
    out[0] = asDWORD(result);
    out[1] = asDWORD(result >> 32);
}

void ModU64To(asDWORD loA, asDWORD hiA, asDWORD loB, asDWORD hiB,
              asDWORD* out) {
    const asQWORD a = asQWORD(loA) | (asQWORD(hiA) << 32);
    const asQWORD b = asQWORD(loB) | (asQWORD(hiB) << 32);
    const asQWORD result = a % b;
    out[0] = asDWORD(result);
    out[1] = asDWORD(result >> 32);
}

void Shift64To(asDWORD lo, asDWORD hi, asDWORD count, int mode,
               asDWORD* out) {
    const asQWORD value = asQWORD(lo) | (asQWORD(hi) << 32);
    asQWORD result = 0;
    if (mode == 0)
        result = value << count;
    else if (mode == 1)
        result = value >> count;
    else
        result = asQWORD(asINT64(value) >> count);
    out[0] = asDWORD(result);
    out[1] = asDWORD(result >> 32);
}

asDWORD U32ToFloatBits(asUINT value) {
    const float converted = float(value);
    asDWORD bits = 0;
    std::memcpy(&bits, &converted, sizeof(bits));
    return bits;
}

asDWORD ModFloatBits(asDWORD leftBits, asDWORD rightBits) {
    float left = 0;
    float right = 0;
    std::memcpy(&left, &leftBits, sizeof(left));
    std::memcpy(&right, &rightBits, sizeof(right));
    const float result = fmodf(left, right);
    asDWORD bits = 0;
    std::memcpy(&bits, &result, sizeof(bits));
    return bits;
}

}

size_t FunctionEmitter::EmitPackedFloatBinary(size_t index) {
    using namespace asmjit;

    if (!useSse_ || cacheLocals_ || index >= instructions_.size()) return 0;
    const asEBCInstr op = instructions_[index].op;
    if (op != asBC_ADDf && op != asBC_SUBf && op != asBC_MULf) return 0;

    auto tryWidth = [&](size_t width, size_t stride) -> size_t {
        const size_t span = (width - 1) * stride + 1;
        if (index + span > instructions_.size()) return 0;

        int firstOffset = 0;
        int firstRight = 0;
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
            const int left = asBC_SWORDARG1(currentIp);
            const int right = asBC_SWORDARG2(currentIp);
            if (destination != left) return 0;
            if (!lane) {
                firstOffset = destination;
                firstRight = right;
            } else if (destination != firstOffset - static_cast<int>(lane) ||
                       right != firstRight - static_cast<int>(lane)) {
                return 0;
            }
        }

        const int lastOffset = firstOffset - static_cast<int>(width - 1);
        const int lastRight = firstRight - static_cast<int>(width - 1);
        const bool rangesOverlap =
            !(firstOffset < lastRight || firstRight < lastOffset);
        if (rangesOverlap && firstOffset != firstRight) return 0;

        auto& cc = Compiler();
        const bool wide = width == 8;
        const x86::Mem leftMemory = wide
            ? x86::ymmword_ptr(fp_, -firstOffset * 4)
            : x86::xmmword_ptr(fp_, -firstOffset * 4);
        const x86::Mem rightMemory = wide
            ? x86::ymmword_ptr(fp_, -firstRight * 4)
            : x86::xmmword_ptr(fp_, -firstRight * 4);
        x86::Vec values = wide
            ? cc.new_ymm_ps("packedFloatValues")
            : cc.new_xmm_ps("packedFloatValues");
        x86::Vec operands = wide
            ? cc.new_ymm_ps("packedFloatOperands")
            : cc.new_xmm_ps("packedFloatOperands");

        if (wide) {
            cc.vmovups(values, leftMemory);
            cc.vmovups(operands, rightMemory);
            if (op == asBC_ADDf)
                cc.vaddps(values, values, operands);
            else if (op == asBC_SUBf)
                cc.vsubps(values, values, operands);
            else
                cc.vmulps(values, values, operands);
            cc.vmovups(leftMemory, values);
        } else {
            cc.movups(values, leftMemory);
            cc.movups(operands, rightMemory);
            if (op == asBC_ADDf)
                cc.addps(values, operands);
            else if (op == asBC_SUBf)
                cc.subps(values, operands);
            else
                cc.mulps(values, operands);
            cc.movups(leftMemory, values);
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
    case asBC_ADDi64:
    case asBC_SUBi64: {
        if (kInlineAdd64) {
            const int destination = asBC_SWORDARG0(ip);
            const int left = asBC_SWORDARG1(ip);
            const int right = asBC_SWORDARG2(ip);
            x86::Gp low = cc.new_gp32("low");
            x86::Gp high = cc.new_gp32("high");
            x86::Gp rightLow = cc.new_gp32("rightLow");
            x86::Gp rightHigh = cc.new_gp32("rightHigh");
            LoadVar(left, low);
            LoadVar(left - 1, high);
            LoadVar(right, rightLow);
            LoadVar(right - 1, rightHigh);
            if (instruction.op == asBC_ADDi64) {
                cc.add(low, rightLow);
                cc.adc(high, rightHigh);
            } else {
                cc.sub(low, rightLow);
                cc.sbb(high, rightHigh);
            }
            StoreVar(destination, low);
            StoreVar(destination - 1, high);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_MULi64: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp aLo = cc.new_gp32("aLo");
        x86::Gp aHi = cc.new_gp32("aHi");
        x86::Gp bLo = cc.new_gp32("bLo");
        x86::Gp bHi = cc.new_gp32("bHi");
        x86::Gp lo = cc.new_gp32("mulLo");
        x86::Gp hi = cc.new_gp32("mulHi");
        x86::Gp t = cc.new_gp32("mulT");
        LoadVar(left, aLo);
        LoadVar(left - 1, aHi);
        LoadVar(right, bLo);
        LoadVar(right - 1, bHi);
        cc.mov(lo, aLo);
        cc.mul(hi, lo, bLo);
        cc.mov(t, aHi);
        cc.imul(t, bLo);
        cc.add(hi, t);
        cc.mov(t, bHi);
        cc.imul(t, aLo);
        cc.add(hi, t);
        StoreVar(destination, lo);
        StoreVar(destination - 1, hi);
        return EmitResult::Success;
    }
    case asBC_NEGi64: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadVar(destination, low);
        LoadVar(destination - 1, high);
        cc.neg(low);
        cc.adc(high, 0);
        cc.neg(high);
        StoreVar(destination, low);
        StoreVar(destination - 1, high);
        return EmitResult::Success;
    }
    case asBC_BNOT64: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadVar(destination, low);
        LoadVar(destination - 1, high);
        cc.not_(low);
        cc.not_(high);
        StoreVar(destination, low);
        StoreVar(destination - 1, high);
        return EmitResult::Success;
    }
    case asBC_BAND64:
    case asBC_BOR64:
    case asBC_BXOR64: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        x86::Gp rightLow = cc.new_gp32("rightLow");
        x86::Gp rightHigh = cc.new_gp32("rightHigh");
        LoadVar(left, low);
        LoadVar(left - 1, high);
        LoadVar(right, rightLow);
        LoadVar(right - 1, rightHigh);
        if (instruction.op == asBC_BAND64) {
            cc.and_(low, rightLow);
            cc.and_(high, rightHigh);
        } else if (instruction.op == asBC_BOR64) {
            cc.or_(low, rightLow);
            cc.or_(high, rightHigh);
        } else {
            cc.xor_(low, rightLow);
            cc.xor_(high, rightHigh);
        }
        StoreVar(destination, low);
        StoreVar(destination - 1, high);
        return EmitResult::Success;
    }
    case asBC_uTOi64: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Gp low = cc.new_gp32("low");
        x86::Gp high = cc.new_gp32("high");
        LoadVar(source, low);
        cc.xor_(high, high);
        StoreVar(destination, low);
        StoreVar(destination - 1, high);
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
        StoreVar(destination, low);
        StoreVar(destination - 1, high);
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
            if (useAvx_ && instruction.op == asBC_ADDf) {
                cc.vaddss(value, value, operand);
            } else if (useAvx_ && instruction.op == asBC_SUBf) {
                cc.vsubss(value, value, operand);
            } else if (useAvx_ && instruction.op == asBC_MULf) {
                cc.vmulss(value, value, operand);
            } else if (instruction.op == asBC_ADDf) {
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
            if (useAvx_ && instruction.op == asBC_ADDd) {
                cc.vaddsd(value, value, operand);
            } else if (useAvx_ && instruction.op == asBC_SUBd) {
                cc.vsubsd(value, value, operand);
            } else if (useAvx_ && instruction.op == asBC_MULd) {
                cc.vmulsd(value, value, operand);
            } else if (instruction.op == asBC_ADDd) {
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
                if (useAvx_)
                    cc.vdivss(value, value, operand);
                else
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
                if (useAvx_)
                    cc.vdivsd(value, value, operand);
                else
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
            Label divideByZero = cc.new_label();
            Label divide = cc.new_label();
            Label done = cc.new_label();
            LoadVar(left, dividend);
            LoadVar(right, divisor);
            cc.test(divisor, divisor);
            cc.jz(divideByZero);
            cc.cmp(divisor, -1);
            cc.jne(divide);
            cc.cmp(dividend, Imm(int64_t(INT32_MIN)));
            cc.jne(divide);
            if (!EmitInternalException(index, ip, TXT_DIVIDE_OVERFLOW))
                return EmitResult::Error;
            cc.bind(divide);
            cc.mov(high, dividend);
            cc.sar(high, 31);
            cc.idiv(high, dividend, divisor);
            StoreVar(destination,
                     instruction.op == asBC_DIVi ? dividend : high);
            cc.jmp(done);

            cc.bind(divideByZero);
            if (!EmitInternalException(index, ip, TXT_DIVIDE_BY_ZERO))
                return EmitResult::Error;
            cc.bind(done);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_DIVu:
    case asBC_MODu: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp dividend = cc.new_gp32("dividend");
        x86::Gp divisor = cc.new_gp32("divisor");
        x86::Gp high = cc.new_gp32("high");
        Label divide = cc.new_label();
        LoadVar(left, dividend);
        LoadVar(right, divisor);
        cc.test(divisor, divisor);
        cc.jnz(divide);
        if (!EmitInternalException(index, ip, TXT_DIVIDE_BY_ZERO))
            return EmitResult::Error;
        cc.bind(divide);
        cc.xor_(high, high);
        cc.div(high, dividend, divisor);
        StoreVar(destination,
                 instruction.op == asBC_DIVu ? dividend : high);
        return EmitResult::Success;
    }
    case asBC_DIVi64:
    case asBC_MODi64: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp aLo = cc.new_gp32("aLo");
        x86::Gp aHi = cc.new_gp32("aHi");
        x86::Gp bLo = cc.new_gp32("bLo");
        x86::Gp bHi = cc.new_gp32("bHi");
        x86::Gp resultLo = cc.new_gp32("resultLo");
        x86::Gp resultHi = cc.new_gp32("resultHi");
        x86::Gp outPtr = cc.new_gp32("outPtr");
        x86::Mem outMem = cc.new_stack(8, 4);
        Label divideByZero = cc.new_label();
        Label divideOverflow = cc.new_label();
        Label divide = cc.new_label();
        Label done = cc.new_label();
        LoadVar(left, aLo);
        LoadVar(left - 1, aHi);
        LoadVar(right, bLo);
        LoadVar(right - 1, bHi);
        cc.mov(resultLo, bLo);
        cc.or_(resultLo, bHi);
        cc.jz(divideByZero);
        cc.cmp(bLo, -1);
        cc.jne(divide);
        cc.test(bHi, bHi);
        cc.jnz(divide);
        cc.test(aLo, aLo);
        cc.jnz(divide);
        cc.cmp(aHi, Imm(int64_t(INT32_MIN)));
        cc.je(divideOverflow);
        cc.bind(divide);
        cc.lea(outPtr, outMem);
        InvokeNode* invocation = nullptr;
        Error err = Invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)(instruction.op == asBC_DIVi64 ? &DivI64To
                                                                 : &ModI64To))),
            FuncSignature::build<void, asDWORD, asDWORD, asDWORD, asDWORD,
                                 asDWORD*>());
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, aLo);
        invocation->set_arg(1, aHi);
        invocation->set_arg(2, bLo);
        invocation->set_arg(3, bHi);
        invocation->set_arg(4, outPtr);
        cc.mov(resultLo, x86::dword_ptr(outPtr));
        cc.mov(resultHi, x86::dword_ptr(outPtr, 4));
        StoreVar(destination, resultLo);
        StoreVar(destination - 1, resultHi);
        cc.jmp(done);
        cc.bind(divideByZero);
        if (!EmitInternalException(index, ip, TXT_DIVIDE_BY_ZERO))
            return EmitResult::Error;
        cc.bind(divideOverflow);
        if (!EmitInternalException(index, ip, TXT_DIVIDE_OVERFLOW))
            return EmitResult::Error;
        cc.bind(done);
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
    case asBC_BNOT: {
        const int destination = asBC_SWORDARG0(ip);
        x86::Gp value = cc.new_gp32("x");
        LoadVar(destination, value);
        cc.not_(value);
        StoreVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_NOT: {
        if (kInlineNot6b) {
            const int destination = asBC_SWORDARG0(ip);
            x86::Gp value = cc.new_gp32("x");
            LoadVar(destination, value);
            cc.test(value.r8(), value.r8());
            cc.set(x86::CondCode::kEqual, value);
            cc.movzx(value, value.r8());
            StoreVar(destination, value);
        } else if (!EmitHelperCall(instruction, ip)) {
            return EmitResult::Error;
        }
        return EmitResult::Success;
    }
    case asBC_TZ:
    case asBC_TNZ:
    case asBC_TS:
    case asBC_TNS:
    case asBC_TP:
    case asBC_TNP: {
        x86::Gp value = cc.new_gp32("testValue");
        cc.mov(value,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
        cc.cmp(value, 0);
        x86::CondCode condition = x86::CondCode::kEqual;
        switch (instruction.op) {
        case asBC_TNZ:
            condition = x86::CondCode::kNotEqual;
            break;
        case asBC_TS:
            condition = x86::CondCode::kSignedLT;
            break;
        case asBC_TNS:
            condition = x86::CondCode::kSignedGE;
            break;
        case asBC_TP:
            condition = x86::CondCode::kSignedGT;
            break;
        case asBC_TNP:
            condition = x86::CondCode::kSignedLE;
            break;
        default:
            break;
        }
        cc.set(condition, value);
        cc.movzx(value, value.r8());
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               value);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister) + 4),
               0);
        return EmitResult::Success;
    }
    case asBC_iTOf: {
        const int offset = asBC_SWORDARG0(ip);
        x86::Gp bits = cc.new_gp32("intBits");
        x86::Vec value = cc.new_xmm_ss("floatValue");
        LoadVar(offset, bits);
        if (useAvx_)
            cc.vcvtsi2ss(value, value, bits);
        else
            cc.cvtsi2ss(value, bits);
        StoreFloatVar(offset, value);
        return EmitResult::Success;
    }
    case asBC_fTOi:
    case asBC_fTOu: {
        const int offset = asBC_SWORDARG0(ip);
        x86::Gp bits = cc.new_gp32("intBits");
        x86::Vec value = cc.new_xmm_ss("floatValue");
        LoadFloatVar(offset, value);
        if (useAvx_)
            cc.vcvttss2si(bits, value);
        else
            cc.cvttss2si(bits, value);
        StoreVar(offset, bits);
        return EmitResult::Success;
    }
    case asBC_dTOf: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Vec value = cc.new_xmm_ss("floatValue");
        LoadDoubleVar(source, value);
        if (useAvx_)
            cc.vcvtsd2ss(value, value, value);
        else
            cc.cvtsd2ss(value, value);
        StoreFloatVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_fTOd: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        x86::Vec value = cc.new_xmm_sd("doubleValue");
        LoadFloatVar(source, value);
        if (useAvx_)
            cc.vcvtss2sd(value, value, value);
        else
            cc.cvtss2sd(value, value);
        StoreDoubleVar(destination, value);
        return EmitResult::Success;
    }
    case asBC_POWi: {
        const int destination = asBC_SWORDARG0(ip);
        const int baseOffset = asBC_SWORDARG1(ip);
        const int expOffset = asBC_SWORDARG2(ip);
        bool constantSquare = false;
        for (size_t lookback = index; lookback > 0;) {
            lookback--;
            const Instruction& previous = instructions_[lookback];
            if (previous.op == asBC_JitEntry) continue;
            if (previous.op != asBC_SetV4) break;
            const asDWORD* setIp = bytecode_ + previous.off;
            if (asBC_SWORDARG0(setIp) != expOffset) break;
            constantSquare = asBC_DWORDARG(setIp) == 2;
            break;
        }
        if (!constantSquare) {
            if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
            return EmitResult::Success;
        }
        x86::Gp base = cc.new_gp32("powBase");
        x86::Gp result = cc.new_gp32("powResult");
        x86::Gp absBase = cc.new_gp32("powAbs");
        Label overflow = cc.new_label();
        Label done = cc.new_label();
        LoadVar(baseOffset, base);
        cc.mov(absBase, base);
        cc.neg(absBase);
        cc.cmov(x86::CondCode::kSign, absBase, base);
        cc.cmp(absBase, 46340);
        cc.ja(overflow);
        cc.mov(result, base);
        cc.imul(result, base);
        StoreVar(destination, result);
        cc.jmp(done);
        cc.bind(overflow);
        if (!EmitInternalException(index, ip, TXT_POW_OVERFLOW))
            return EmitResult::Error;
        cc.bind(done);
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
    case asBC_CMPu:
    case asBC_CMPIu:
    case asBC_CmpPtr: {
        if (kInlineCmp6c) {
            const int left = asBC_SWORDARG0(ip);
            const bool isUnsigned = instruction.op == asBC_CmpPtr ||
                                    instruction.op == asBC_CMPu ||
                                    instruction.op == asBC_CMPIu;
            x86::Gp x = cc.new_gp32("x");
            LoadVar(left, x);
            x86::Gp y = cc.new_gp32("y");
            if (instruction.op == asBC_CMPi ||
                instruction.op == asBC_CmpPtr ||
                instruction.op == asBC_CMPu) {
                LoadVar(asBC_SWORDARG1(ip), y);
                cc.cmp(x, y);
            } else if (instruction.op == asBC_CMPIu) {
                cc.cmp(x, Imm(int64_t(asBC_DWORDARG(ip))));
            } else {
                cc.cmp(x, Imm(int64_t(asBC_INTARG(ip))));
            }
            if (fusedCmpBranch_[index]) {
                const size_t branchIndex =
                    index + fusedCmpBranch_[index];
                const Instruction& branch = instructions_[branchIndex];
                asEBCInstr branchOp = branch.op;
                if (fusedInvertBranch_[index]) {
                    switch (branchOp) {
                    case asBC_JZ:
                        branchOp = asBC_JNZ;
                        break;
                    case asBC_JNZ:
                        branchOp = asBC_JZ;
                        break;
                    case asBC_JLowZ:
                        branchOp = asBC_JLowNZ;
                        break;
                    case asBC_JLowNZ:
                        branchOp = asBC_JLowZ;
                        break;
                    default:
                        break;
                    }
                }
                const int targetIndex = BranchTargetIndex(
                    branch, bytecode_ + branch.off);
                if (targetIndex < 0) return EmitResult::Error;
                switch (branchOp) {
                case asBC_JZ:
                    cc.jz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNZ:
                    cc.jnz(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JS:
                    if (isUnsigned)
                        cc.jb(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.js(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNS:
                    if (isUnsigned)
                        cc.jae(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.jns(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JP:
                    if (isUnsigned)
                        cc.ja(labels_[static_cast<size_t>(targetIndex)]);
                    else
                        cc.jg(labels_[static_cast<size_t>(targetIndex)]);
                    break;
                case asBC_JNP:
                    if (isUnsigned)
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
                cc.set(isUnsigned ? x86::CondCode::kUnsignedGT
                                  : x86::CondCode::kSignedGT,
                       x);
                cc.set(isUnsigned ? x86::CondCode::kUnsignedLT
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
    case asBC_CMPi64:
    case asBC_CMPu64: {
        const int left = asBC_SWORDARG0(ip);
        const int right = asBC_SWORDARG1(ip);
        const bool isUnsigned = instruction.op == asBC_CMPu64;
        x86::Gp leftLo = cc.new_gp32("leftLo");
        x86::Gp leftHi = cc.new_gp32("leftHi");
        x86::Gp rightLo = cc.new_gp32("rightLo");
        x86::Gp rightHi = cc.new_gp32("rightHi");
        x86::Gp result = cc.new_gp32("result");
        Label less = cc.new_label();
        Label greater = cc.new_label();
        Label store = cc.new_label();
        LoadVar(left, leftLo);
        LoadVar(left - 1, leftHi);
        LoadVar(right, rightLo);
        LoadVar(right - 1, rightHi);
        cc.cmp(leftHi, rightHi);
        if (isUnsigned)
            cc.jb(less);
        else
            cc.jl(less);
        if (isUnsigned)
            cc.ja(greater);
        else
            cc.jg(greater);
        cc.cmp(leftLo, rightLo);
        cc.jb(less);
        cc.ja(greater);
        cc.xor_(result, result);
        cc.jmp(store);
        cc.bind(less);
        cc.mov(result, -1);
        cc.jmp(store);
        cc.bind(greater);
        cc.mov(result, 1);
        cc.bind(store);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               result);
        return EmitResult::Success;
    }
    case asBC_BSLL64:
    case asBC_BSRL64:
    case asBC_BSRA64: {
        const int destination = asBC_SWORDARG0(ip);
        const int source = asBC_SWORDARG1(ip);
        const int countOffset = asBC_SWORDARG2(ip);
        x86::Gp lo = cc.new_gp32("lo");
        x86::Gp hi = cc.new_gp32("hi");
        x86::Gp count = cc.new_gp32("count");
        LoadVar(source, lo);
        LoadVar(source - 1, hi);
        LoadVar(countOffset, count);
        // Same shld/shrd pair the C++ compiler emits for the interpreter's
        // 64-bit shifts, so oversized counts behave identically.
        if (instruction.op == asBC_BSLL64) {
            cc.shld(hi, lo, count);
            cc.shl(lo, count);
        } else {
            cc.shrd(lo, hi, count);
            if (instruction.op == asBC_BSRL64)
                cc.shr(hi, count);
            else
                cc.sar(hi, count);
        }
        StoreVar(destination, lo);
        StoreVar(destination - 1, hi);
        return EmitResult::Success;
    }
    case asBC_DIVu64:
    case asBC_MODu64: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp aLo = cc.new_gp32("aLo");
        x86::Gp aHi = cc.new_gp32("aHi");
        x86::Gp bLo = cc.new_gp32("bLo");
        x86::Gp bHi = cc.new_gp32("bHi");
        x86::Gp resultLo = cc.new_gp32("resultLo");
        x86::Gp outPtr = cc.new_gp32("outPtr");
        x86::Mem outMem = cc.new_stack(8, 4);
        Label divideByZero = cc.new_label();
        Label done = cc.new_label();
        LoadVar(left, aLo);
        LoadVar(left - 1, aHi);
        LoadVar(right, bLo);
        LoadVar(right - 1, bHi);
        cc.mov(resultLo, bLo);
        cc.or_(resultLo, bHi);
        cc.jz(divideByZero);
        cc.lea(outPtr, outMem);
        InvokeNode* invocation = nullptr;
        Error err = Invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)(instruction.op == asBC_DIVu64 ? &DivU64To
                                                                 : &ModU64To))),
            FuncSignature::build<void, asDWORD, asDWORD, asDWORD, asDWORD,
                                 asDWORD*>());
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, aLo);
        invocation->set_arg(1, aHi);
        invocation->set_arg(2, bLo);
        invocation->set_arg(3, bHi);
        invocation->set_arg(4, outPtr);
        cc.mov(aLo, x86::dword_ptr(outPtr));
        cc.mov(aHi, x86::dword_ptr(outPtr, 4));
        StoreVar(destination, aLo);
        StoreVar(destination - 1, aHi);
        cc.jmp(done);
        cc.bind(divideByZero);
        if (!EmitInternalException(index, ip, TXT_DIVIDE_BY_ZERO))
            return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    case asBC_uTOf: {
        const int offset = asBC_SWORDARG0(ip);
        x86::Gp bits = cc.new_gp32("bits");
        x86::Gp odd = cc.new_gp32("oddBit");
        x86::Gp halved = cc.new_gp32("halved");
        x86::Vec value = cc.new_xmm_ss("value");
        Label negative = cc.new_label();
        Label done = cc.new_label();
        LoadVar(offset, bits);
        // Values >= 0x80000000 convert as (v>>1 | v&1) doubled, matching
        // the interpreter's unsigned conversion.
        cc.test(bits, bits);
        cc.js(negative);
        if (useAvx_)
            cc.vcvtsi2ss(value, value, bits);
        else
            cc.cvtsi2ss(value, bits);
        cc.jmp(done);
        cc.bind(negative);
        cc.mov(odd, bits);
        cc.and_(odd, 1);
        cc.mov(halved, bits);
        cc.shr(halved, 1);
        cc.or_(halved, odd);
        if (useAvx_)
            cc.vcvtsi2ss(value, value, halved);
        else
            cc.cvtsi2ss(value, halved);
        if (useAvx_)
            cc.vaddss(value, value, value);
        else
            cc.addss(value, value);
        cc.bind(done);
        if (useAvx_)
            cc.vmovd(bits, value);
        else
            cc.movd(bits, value);
        StoreVar(offset, bits);
        return EmitResult::Success;
    }
    case asBC_ClrHi: {
        x86::Gp value = cc.new_gp32("value");
        cc.mov(value,
               x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)));
        cc.movzx(value, value.r8());
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister)),
               value);
        cc.mov(x86::dword_ptr(
                   regs_, offsetof(asSVMRegisters, valueRegister) + 4),
               0);
        return EmitResult::Success;
    }
    case asBC_MODf: {
        const int destination = asBC_SWORDARG0(ip);
        const int left = asBC_SWORDARG1(ip);
        const int right = asBC_SWORDARG2(ip);
        x86::Gp divisorBits = cc.new_gp32("divisorBits");
        x86::Gp leftBits = cc.new_gp32("leftBits");
        x86::Gp rightBits = cc.new_gp32("rightBits");
        x86::Gp resultBits = cc.new_gp32("resultBits");
        Label fallback = cc.new_label();
        Label done = cc.new_label();
        LoadVar(right, divisorBits);
        cc.and_(divisorBits, 0x7FFFFFFF);
        cc.jz(fallback);
        LoadVar(left, leftBits);
        LoadVar(right, rightBits);
        InvokeNode* invocation = nullptr;
        Error err = Invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&ModFloatBits)),
            FuncSignature::build<asDWORD, asDWORD, asDWORD>());
        if (err != kErrorOk) return EmitResult::Error;
        invocation->set_arg(0, leftBits);
        invocation->set_arg(1, rightBits);
        invocation->set_ret(0, resultBits);
        StoreVar(destination, resultBits);
        cc.jmp(done);
        cc.bind(fallback);
        if (!EmitHelperCall(instruction, ip)) return EmitResult::Error;
        cc.bind(done);
        return EmitResult::Success;
    }
    default:
        return EmitResult::Unhandled;
    }
}

}
