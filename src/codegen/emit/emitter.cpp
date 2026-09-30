#include "codegen/emit/emitter.h"

#include "bytecode/bc_helpers.h"
#include "bytecode/helpers/runtime_helpers.h"

#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

namespace asjitx86::emit {

FunctionEmitter::FunctionEmitter(asmjit::JitRuntime& runtime,
                                 detail::ScalarObjectPool& objectPool,
                                 asIScriptFunction* function,
                                 asJITFunction* out)
    : runtime_(runtime), objectPool_(objectPool), function_(function),
      out_(out) {
#if ASJITX86_ENABLE_SSE
    useSse_ = runtime_.cpu_features().x86().has_sse2();
#endif
#if ASJITX86_ENABLE_AVX2
    useAvx_ = useSse_ && runtime_.cpu_features().x86().has_avx2();
#endif
}

int FunctionEmitter::Run() {
    *out_ = nullptr;
    if (!AnalyzeBytecode() || !InitializeCompiler() || !EmitEntryDispatch() ||
        !EmitInstructions() || !Finalize())
        return asERROR;
    return asSUCCESS;
}

bool FunctionEmitter::InitializeCompiler() {
    using namespace asmjit;

    Error err = code_.init(runtime_.environment(), runtime_.cpu_features());
    if (err != kErrorOk) return false;

    if (getenv("ASJITX86_LOG")) {
        logger_ = std::make_unique<FileLogger>(fopen("asjitx86.log", "a"));
        code_.set_logger(logger_.get());
    }

    compiler_ = std::make_unique<x86::Compiler>(&code_);
    auto& cc = Compiler();
    FuncNode* fnNode = cc.add_func(
        FuncSignature::build<void, asSVMRegisters*, asPWORD>());
    if (!fnNode) return false;
    if (useAvx_) {
        fnNode->frame().set_avx_enabled();
        fnNode->frame().set_avx_auto_cleanup();
    }

    regs_ = cc.new_gp32("regs");
    jitArg_ = cc.new_gp32("jitArg");
    fnNode->set_arg(0, regs_);
    fnNode->set_arg(1, jitArg_);

    fp_ = cc.new_gp32("fp");
    cc.mov(fp_, x86::dword_ptr(
                    regs_, offsetof(asSVMRegisters, stackFramePointer)));

    if (cacheLocals_) {
        cachedLocals_.reserve(cachedLocalOffsets_.size());
        for (size_t index = 0; index < cachedLocalOffsets_.size(); index++) {
            cachedLocals_.push_back(cc.new_gp32("cachedLocal"));
            cc.mov(cachedLocals_[index],
                   x86::dword_ptr(
                       fp_, -cachedLocalOffsets_[index] * 4));
        }
    }

    labels_.resize(instructions_.size());
    for (size_t i = 0; i < instructions_.size(); i++) {
        if (needsLabel_[i]) labels_[i] = cc.new_label();
    }
    exitLabel_ = cc.new_label();
    return true;
}

bool FunctionEmitter::EmitInstructions() {
    auto& cc = Compiler();
    for (size_t i = 0; i < instructions_.size(); i++) {
        if (i > 0 && fusedCmpBranch_[i - 1]) continue;
        if (i > 1 && fusedCmpBranch_[i - 2] == 2) continue;
        if (refCopyFusionSkip_[i]) continue;
        if (needsLabel_[i]) cc.bind(labels_[i]);
        const size_t packedBinaryCount = EmitPackedFloatBinary(i);
        if (packedBinaryCount) {
            i += packedBinaryCount - 1;
            continue;
        }
        const size_t packedCount = EmitPackedFloatImmediate(i);
        if (packedCount) {
            i += packedCount - 1;
            continue;
        }
        const Instruction& instruction = instructions_[i];
        const asDWORD* ip = bytecode_ + instruction.off;
        vrProduce_ = vrForwardSpan_[i] != 0;
        vrConsume_ = vrForwardConsume_[i] != 0;
        vrDeadAfter_ = vrForwardDead_[i] != 0;
        if (!EmitInstruction(i, instruction, ip)) return false;
        if (vrConsume_) FinishValueRegisterForward();
        vrProduce_ = false;
        vrConsume_ = false;
    }
    return true;
}

bool FunctionEmitter::EmitInstruction(size_t index,
                                      const Instruction& instruction,
                                      const asDWORD* ip) {
    EmitResult result = EmitControlFlow(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitStack(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitCalls(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitReferences(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitMemory(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitNumeric(index, instruction, ip);
    if (result == EmitResult::Unhandled)
        result = EmitHelperCall(instruction, ip) ? EmitResult::Success
                                                 : EmitResult::Error;
    return result == EmitResult::Success;
}

int FunctionEmitter::CachedLocalSlot(int offset) const {
    if (!cacheLocals_) return -1;
    const auto begin = cachedLocalOffsets_.begin();
    const auto end = cachedLocalOffsets_.end();
    const auto found = std::lower_bound(begin, end, offset);
    if (found == end || *found != offset) return -1;
    return static_cast<int>(found - begin);
}

void FunctionEmitter::LoadVar(int offset,
                              const asmjit::x86::Gp& destination) {
    const int slot = CachedLocalSlot(offset);
    if (slot >= 0)
        Compiler().mov(destination, cachedLocals_[static_cast<size_t>(slot)]);
    else
        Compiler().mov(destination,
                       asmjit::x86::dword_ptr(fp_, -offset * 4));
}

void FunctionEmitter::StoreVar(int offset, const asmjit::x86::Gp& source) {
    const int slot = CachedLocalSlot(offset);
    if (slot >= 0)
        Compiler().mov(cachedLocals_[static_cast<size_t>(slot)], source);
    else
        Compiler().mov(asmjit::x86::dword_ptr(fp_, -offset * 4), source);
}

void FunctionEmitter::LoadVar64(int offset,
                                const asmjit::x86::Vec& destination) {
    using namespace asmjit;
    auto& cc = Compiler();
    const int slot = CachedLocalSlot(offset);
    const int highSlot = CachedLocalSlot(offset - 1);
    if (slot < 0 && highSlot < 0) {
        cc.movq(destination, x86::qword_ptr(fp_, -offset * 4));
        return;
    }
    // At least one half lives in a register; assemble both halves from
    // their authoritative locations so a half-cached pair stays coherent.
    x86::Gp low = cc.new_gp32("cachedLow64");
    x86::Gp high = cc.new_gp32("cachedHigh64");
    x86::Vec packed = cc.new_xmm("cachedPack64");
    if (slot >= 0)
        cc.mov(low, cachedLocals_[static_cast<size_t>(slot)]);
    else
        cc.mov(low, x86::dword_ptr(fp_, -offset * 4));
    if (highSlot >= 0)
        cc.mov(high, cachedLocals_[static_cast<size_t>(highSlot)]);
    else
        cc.mov(high, x86::dword_ptr(fp_, -(offset - 1) * 4));
    cc.movd(destination, low);
    cc.movd(packed, high);
    cc.psllq(packed, 32);
    cc.por(destination, packed);
}

void FunctionEmitter::StoreVar64(int offset,
                                 const asmjit::x86::Vec& source) {
    using namespace asmjit;
    auto& cc = Compiler();
    const int slot = CachedLocalSlot(offset);
    const int highSlot = CachedLocalSlot(offset - 1);
    if (slot < 0 && highSlot < 0) {
        cc.movq(x86::qword_ptr(fp_, -offset * 4), source);
        return;
    }
    // Route each half to its authoritative location so the cached low
    // half can never go stale behind the qword memory write.
    x86::Gp low = cc.new_gp32("cachedLow64");
    x86::Gp high = cc.new_gp32("cachedHigh64");
    x86::Vec shifted = cc.new_xmm("cachedShift64");
    cc.movd(low, source);
    cc.movq(shifted, source);
    cc.psrlq(shifted, 32);
    cc.movd(high, shifted);
    if (slot >= 0)
        cc.mov(cachedLocals_[static_cast<size_t>(slot)], low);
    else
        cc.mov(x86::dword_ptr(fp_, -offset * 4), low);
    if (highSlot >= 0)
        cc.mov(cachedLocals_[static_cast<size_t>(highSlot)], high);
    else
        cc.mov(x86::dword_ptr(fp_, -(offset - 1) * 4), high);
}

void FunctionEmitter::LoadFloatVar(
    int offset, const asmjit::x86::Vec& destination) {
    using namespace asmjit;
    auto& cc = Compiler();
    if (useSse_ && CachedLocalSlot(offset) < 0) {
        const x86::Mem source = x86::dword_ptr(fp_, -offset * 4);
        if (useAvx_)
            cc.vmovss(destination, source);
        else
            cc.movss(destination, source);
        return;
    }

    x86::Gp bits = cc.new_gp32("floatBits");
    LoadVar(offset, bits);
    if (useAvx_)
        cc.vmovd(destination, bits);
    else
        cc.movd(destination, bits);
}

void FunctionEmitter::StoreFloatVar(
    int offset, const asmjit::x86::Vec& source) {
    using namespace asmjit;
    auto& cc = Compiler();
    if (useSse_ && CachedLocalSlot(offset) < 0) {
        const x86::Mem destination = x86::dword_ptr(fp_, -offset * 4);
        if (useAvx_)
            cc.vmovss(destination, source);
        else
            cc.movss(destination, source);
        return;
    }

    x86::Gp bits = cc.new_gp32("floatBits");
    if (useAvx_)
        cc.vmovd(bits, source);
    else
        cc.movd(bits, source);
    StoreVar(offset, bits);
}

void FunctionEmitter::LoadDoubleVar(
    int offset, const asmjit::x86::Vec& destination) {
    using namespace asmjit;
    if (useSse_ && CachedLocalSlot(offset) < 0) {
        const x86::Mem source = x86::qword_ptr(fp_, -offset * 4);
        if (useAvx_)
            Compiler().vmovsd(destination, source);
        else
            Compiler().movsd(destination, source);
        return;
    }
    LoadVar64(offset, destination);
}

void FunctionEmitter::StoreDoubleVar(
    int offset, const asmjit::x86::Vec& source) {
    using namespace asmjit;
    if (useSse_ && CachedLocalSlot(offset) < 0) {
        const x86::Mem destination = x86::qword_ptr(fp_, -offset * 4);
        if (useAvx_)
            Compiler().vmovsd(destination, source);
        else
            Compiler().movsd(destination, source);
        return;
    }
    StoreVar64(offset, source);
}

void FunctionEmitter::FlushCachedLocals() {
    if (!cacheLocals_) return;
    auto& cc = Compiler();
    for (size_t index = 0; index < cachedLocals_.size(); index++)
        cc.mov(asmjit::x86::dword_ptr(
                   fp_, -cachedLocalOffsets_[index] * 4),
               cachedLocals_[index]);
}

void FunctionEmitter::ReloadCachedLocals() {
    if (!cacheLocals_) return;
    auto& cc = Compiler();
    for (size_t index = 0; index < cachedLocals_.size(); index++)
        cc.mov(cachedLocals_[index],
               asmjit::x86::dword_ptr(
                   fp_, -cachedLocalOffsets_[index] * 4));
}

void FunctionEmitter::LoadSp(const asmjit::x86::Gp& destination) {
    Compiler().mov(destination,
                   asmjit::x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, stackPointer)));
}

void FunctionEmitter::StoreSp(const asmjit::x86::Gp& source) {
    Compiler().mov(
        asmjit::x86::dword_ptr(regs_, offsetof(asSVMRegisters, stackPointer)),
        source);
}

void FunctionEmitter::StoreValueRegisterForwarded(
    const asmjit::x86::Gp& value) {
    if (!vrProduce_) {
        Compiler().mov(
            asmjit::x86::dword_ptr(
                regs_, offsetof(asSVMRegisters, valueRegister)),
            value);
        return;
    }
    vrShadow_ = Compiler().new_gp32("vrShadow");
    Compiler().mov(vrShadow_, value);
    vrActive_ = true;
}

void FunctionEmitter::LoadValueRegisterForwarded(
    const asmjit::x86::Gp& destination) {
    if (vrActive_ && vrConsume_) {
        Compiler().mov(destination, vrShadow_);
        return;
    }
    Compiler().mov(destination,
                   asmjit::x86::dword_ptr(
                       regs_, offsetof(asSVMRegisters, valueRegister)));
}

void FunctionEmitter::FinishValueRegisterForward() {
    if (!vrActive_) return;
    if (!vrDeadAfter_)
        Compiler().mov(
            asmjit::x86::dword_ptr(
                regs_, offsetof(asSVMRegisters, valueRegister)),
            vrShadow_);
    vrActive_ = false;
}

bool FunctionEmitter::EmitHelperCall(const Instruction& instruction,
                                     const asDWORD* ip) {
    using namespace asmjit;

    JitBcHelper helper = GetJitBcHelper(instruction.op);
    if (!helper) return false;
    auto& cc = Compiler();
    FlushCachedLocals();
    InvokeNode* invocation = nullptr;
    Error err = cc.invoke(Out<InvokeNode*>(invocation),
                          Imm(int64_t((intptr_t)helper)),
                          FuncSignature::build<int, asSVMRegisters*,
                                               const asDWORD*>());
    if (err != kErrorOk) return false;
    x86::Gp result = cc.new_gp32("res");
    invocation->set_arg(0, regs_);
    invocation->set_arg(1, Imm(int64_t((intptr_t)ip)));
    invocation->set_ret(0, result);
    if (instruction.op != asBC_RET) ReloadCachedLocals();
    cc.test(result, result);
    cc.jnz(exitLabel_);
    return true;
}

bool FunctionEmitter::EmitInternalException(size_t index, const asDWORD* ip,
                                            const char* message) {
    using namespace asmjit;

    auto& cc = Compiler();
    FlushCachedLocals();
    InvokeNode* invocation = nullptr;
    const int catchTarget = localCatchTarget_[index];
    if (catchTarget >= 0) {
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&detail::RaiseAndCatchInternalException)),
            FuncSignature::build<int, asSVMRegisters*, const asDWORD*,
                                 const char*, asCScriptFunction*,
                                 const asSTryCatchInfo*, int>());
        if (err != kErrorOk) return false;
        x86::Gp result = cc.new_gp32("exceptionResult");
        invocation->set_arg(0, regs_);
        invocation->set_arg(1, Imm(int64_t((intptr_t)ip)));
        invocation->set_arg(2, Imm(int64_t((intptr_t)message)));
        invocation->set_arg(3, Imm(int64_t((intptr_t)scriptFunction_)));
        invocation->set_arg(
            4, Imm(int64_t((intptr_t)localCatchInfo_[index])));
        invocation->set_arg(5, localCatchNeedsCleanup_[index]);
        invocation->set_ret(0, result);
        cc.test(result, result);
        cc.jnz(exitLabel_);
        // Exception cleanup may have rewritten this frame's locals before
        // control resumes at the catch block.
        ReloadCachedLocals();
        cc.jmp(labels_[static_cast<size_t>(catchTarget)]);
    } else {
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&detail::RaiseInternalException)),
            FuncSignature::build<void, asSVMRegisters*, const asDWORD*,
                                 const char*>());
        if (err != kErrorOk) return false;
        invocation->set_arg(0, regs_);
        invocation->set_arg(1, Imm(int64_t((intptr_t)ip)));
        invocation->set_arg(2, Imm(int64_t((intptr_t)message)));
        cc.jmp(exitLabel_);
    }
    return true;
}

int FunctionEmitter::BranchTargetIndex(const Instruction& instruction,
                                       const asDWORD* ip) const {
    const int64_t target = int64_t(instruction.off) + 2 + asBC_INTARG(ip);
    if (target < 0 || target >= int64_t(bytecodeLength_)) return -1;
    return indexOfOffset_[static_cast<size_t>(target)];
}

asmjit::x86::Compiler& FunctionEmitter::Compiler() {
    return *compiler_;
}

bool FunctionEmitter::Finalize() {
    using namespace asmjit;

    auto& cc = Compiler();
    cc.bind(exitLabel_);
    FlushCachedLocals();
    cc.end_func();
    Error err = cc.finalize();
    if (err != kErrorOk) return false;

    asJITFunction compiledFunction = nullptr;
    err = runtime_.add(&compiledFunction, &code_);
    if (err != kErrorOk) return false;
    if (logger_) {
        fflush(nullptr);
        fclose(logger_->_file);
        logger_.reset();
    }

    asPWORD entryId = 1;
    for (const Instruction& instruction : instructions_) {
        if (instruction.op == asBC_JitEntry)
            *(asPWORD*)(bytecode_ + instruction.off + 1) = entryId++;
    }

    *out_ = compiledFunction;
    return true;
}

}
