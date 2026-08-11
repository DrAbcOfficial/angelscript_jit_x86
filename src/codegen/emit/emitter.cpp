#include "codegen/emit/emitter.h"

#include "bytecode/bc_helpers.h"
#include "bytecode/helpers/runtime_helpers.h"

#include "as_scriptengine.h"
#include "as_scriptfunction.h"

#include <cstddef>

namespace asjitx86::emit {

FunctionEmitter::FunctionEmitter(asmjit::JitRuntime& runtime,
                                 asIScriptFunction* function,
                                 asJITFunction* out)
    : runtime_(runtime), function_(function), out_(out) {}

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

    compiler_ = std::make_unique<x86::Compiler>(&code_);
    auto& cc = Compiler();
    FuncNode* fnNode = cc.add_func(
        FuncSignature::build<void, asSVMRegisters*, asPWORD>());
    if (!fnNode) return false;

    regs_ = cc.new_gp32("regs");
    jitArg_ = cc.new_gp32("jitArg");
    fnNode->set_arg(0, regs_);
    fnNode->set_arg(1, jitArg_);

    fp_ = cc.new_gp32("fp");
    cc.mov(fp_, x86::dword_ptr(
                    regs_, offsetof(asSVMRegisters, stackFramePointer)));

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
        if (refCopyFusionSkip_[i]) continue;
        if (needsLabel_[i]) cc.bind(labels_[i]);
        const Instruction& instruction = instructions_[i];
        const asDWORD* ip = bytecode_ + instruction.off;
        if (!EmitInstruction(i, instruction, ip)) return false;
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

void FunctionEmitter::LoadVar(int offset,
                              const asmjit::x86::Gp& destination) {
    Compiler().mov(destination, asmjit::x86::dword_ptr(fp_, -offset * 4));
}

void FunctionEmitter::StoreVar(int offset, const asmjit::x86::Gp& source) {
    Compiler().mov(asmjit::x86::dword_ptr(fp_, -offset * 4), source);
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

bool FunctionEmitter::EmitHelperCall(const Instruction& instruction,
                                     const asDWORD* ip) {
    using namespace asmjit;

    JitBcHelper helper = GetJitBcHelper(instruction.op);
    if (!helper) return false;
    auto& cc = Compiler();
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
    cc.test(result, result);
    cc.jnz(exitLabel_);
    return true;
}

bool FunctionEmitter::EmitInternalException(size_t index, const asDWORD* ip,
                                            const char* message) {
    using namespace asmjit;

    auto& cc = Compiler();
    InvokeNode* invocation = nullptr;
    const int catchTarget = localCatchTarget_[index];
    if (catchTarget >= 0) {
        Error err = cc.invoke(
            Out<InvokeNode*>(invocation),
            Imm(int64_t((intptr_t)&detail::RaiseAndCatchInternalException)),
            FuncSignature::build<int, asSVMRegisters*, const asDWORD*,
                                 const char*, asCScriptFunction*,
                                 const asDWORD*>());
        if (err != kErrorOk) return false;
        x86::Gp result = cc.new_gp32("exceptionResult");
        const asDWORD* catchBc =
            bytecode_ + instructions_[static_cast<size_t>(catchTarget)].off;
        invocation->set_arg(0, regs_);
        invocation->set_arg(1, Imm(int64_t((intptr_t)ip)));
        invocation->set_arg(2, Imm(int64_t((intptr_t)message)));
        invocation->set_arg(3, Imm(int64_t((intptr_t)scriptFunction_)));
        invocation->set_arg(4, Imm(int64_t((intptr_t)catchBc)));
        invocation->set_ret(0, result);
        cc.test(result, result);
        cc.jnz(exitLabel_);
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
    cc.end_func();
    Error err = cc.finalize();
    if (err != kErrorOk) return false;

    asJITFunction compiledFunction = nullptr;
    err = runtime_.add(&compiledFunction, &code_);
    if (err != kErrorOk) return false;

    asPWORD entryId = 1;
    for (const Instruction& instruction : instructions_) {
        if (instruction.op == asBC_JitEntry)
            *(asPWORD*)(bytecode_ + instruction.off + 1) = entryId++;
    }

    *out_ = compiledFunction;
    return true;
}

}
