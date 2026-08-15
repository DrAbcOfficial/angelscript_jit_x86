#pragma once

#include "angelscript.h"

#include <asmjit/x86.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class asCScriptEngine;
class asCScriptFunction;

namespace asjitx86::detail {
class ScalarObjectPool;
}

namespace asjitx86::emit {

struct Instruction {
    asEBCInstr op;
    uint32_t off;
    uint32_t size;
};

enum class EmitResult {
    Unhandled,
    Success,
    Error,
};

class FunctionEmitter final {
public:
    FunctionEmitter(asmjit::JitRuntime& runtime,
                    detail::ScalarObjectPool& objectPool,
                    asIScriptFunction* function, asJITFunction* out);

    int Run();

private:
    bool AnalyzeBytecode();
    bool DecodeInstructions();
    bool AnalyzeLabels();
    bool AnalyzeCatchTargets();
    void AnalyzeReferenceCopyFusions();
    bool AnalyzeComparisonBranchFusions();
    bool IsValueRegisterDeadFrom(size_t start) const;

    bool InitializeCompiler();
    bool EmitEntryDispatch();
    bool EmitInstructions();
    bool EmitInstruction(size_t index, const Instruction& instruction,
                         const asDWORD* ip);
    bool Finalize();

    EmitResult EmitControlFlow(size_t index, const Instruction& instruction,
                               const asDWORD* ip);
    EmitResult EmitStack(size_t index, const Instruction& instruction,
                         const asDWORD* ip);
    EmitResult EmitCalls(size_t index, const Instruction& instruction,
                         const asDWORD* ip);
    EmitResult EmitReferences(size_t index, const Instruction& instruction,
                              const asDWORD* ip);
    EmitResult EmitMemory(size_t index, const Instruction& instruction,
                          const asDWORD* ip);
    EmitResult EmitNumeric(size_t index, const Instruction& instruction,
                           const asDWORD* ip);

    void LoadVar(int offset, const asmjit::x86::Gp& destination);
    void StoreVar(int offset, const asmjit::x86::Gp& source);
    void LoadVar64(int offset, const asmjit::x86::Vec& destination);
    void StoreVar64(int offset, const asmjit::x86::Vec& source);
    void FlushCachedLocals();
    void ReloadCachedLocals();
    void LoadSp(const asmjit::x86::Gp& destination);
    void StoreSp(const asmjit::x86::Gp& source);
    bool EmitHelperCall(const Instruction& instruction, const asDWORD* ip);
    bool EmitInternalException(size_t index, const asDWORD* ip,
                               const char* message);

    int BranchTargetIndex(const Instruction& instruction,
                          const asDWORD* ip) const;

    asmjit::x86::Compiler& Compiler();

    asmjit::JitRuntime& runtime_;
    detail::ScalarObjectPool& objectPool_;
    asIScriptFunction* function_;
    asJITFunction* out_;
    asCScriptEngine* engine_ = nullptr;
    asCScriptFunction* scriptFunction_ = nullptr;
    asDWORD* bytecode_ = nullptr;
    asUINT bytecodeLength_ = 0;
    bool inlineFieldMemory_ = false;
    bool cacheLocals_ = false;

    std::vector<Instruction> instructions_;
    std::vector<int> indexOfOffset_;
    std::vector<uint8_t> needsLabel_;
    std::vector<int> localCatchTarget_;
    std::vector<uint8_t> refCopyFusionSpan_;
    std::vector<uint8_t> refCopyFusionSkip_;
    std::vector<uint8_t> fusedCmpBranch_;
    std::vector<int8_t> fusedFallValue_;

    asmjit::CodeHolder code_;
    std::unique_ptr<asmjit::x86::Compiler> compiler_;
    asmjit::x86::Gp regs_;
    asmjit::x86::Gp jitArg_;
    asmjit::x86::Gp fp_;
    std::vector<asmjit::x86::Gp> cachedLocals_;
    std::vector<asmjit::Label> labels_;
    asmjit::Label exitLabel_;
};

}
