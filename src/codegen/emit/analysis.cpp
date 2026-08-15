#include "codegen/emit/emitter.h"

#include "bytecode/bc_info.h"

#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptfunction.h"

namespace asjitx86::emit {

namespace {

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

bool IsCacheableLocalOp(asEBCInstr op) {
    switch (op) {
    case asBC_JitEntry:
    case asBC_SUSPEND:
    case asBC_JMP:
    case asBC_JZ:
    case asBC_JNZ:
    case asBC_JS:
    case asBC_JNS:
    case asBC_JP:
    case asBC_JNP:
    case asBC_JLowZ:
    case asBC_JLowNZ:
    case asBC_RET:
    case asBC_SetV1:
    case asBC_SetV2:
    case asBC_SetV4:
    case asBC_SetV8:
    case asBC_CpyVtoV4:
    case asBC_CpyVtoV8:
    case asBC_CpyVtoR4:
    case asBC_CpyVtoR8:
    case asBC_CpyRtoV4:
    case asBC_CpyRtoV8:
    case asBC_iTOf:
    case asBC_fTOi:
    case asBC_uTOf:
    case asBC_fTOu:
    case asBC_dTOi:
    case asBC_dTOu:
    case asBC_dTOf:
    case asBC_iTOd:
    case asBC_uTOd:
    case asBC_fTOd:
    case asBC_ADDi:
    case asBC_SUBi:
    case asBC_MULi:
    case asBC_DIVi:
    case asBC_MODi:
    case asBC_ADDf:
    case asBC_SUBf:
    case asBC_MULf:
    case asBC_DIVf:
    case asBC_ADDd:
    case asBC_SUBd:
    case asBC_MULd:
    case asBC_DIVd:
    case asBC_ADDIi:
    case asBC_SUBIi:
    case asBC_MULIi:
    case asBC_ADDIf:
    case asBC_SUBIf:
    case asBC_MULIf:
    case asBC_NEGi:
    case asBC_NEGf:
    case asBC_NEGd:
    case asBC_NOT:
    case asBC_BNOT:
    case asBC_BAND:
    case asBC_BOR:
    case asBC_BXOR:
    case asBC_BSLL:
    case asBC_BSRL:
    case asBC_BSRA:
    case asBC_IncVi:
    case asBC_DecVi:
    case asBC_CMPi:
    case asBC_CMPIi:
    case asBC_CMPf:
    case asBC_CMPd:
    case asBC_CMPIf:
    case asBC_TZ:
    case asBC_TNZ:
    case asBC_TS:
    case asBC_TNS:
    case asBC_TP:
    case asBC_TNP:
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
    case asBC_LOADOBJ:
    case asBC_INCi:
    case asBC_DECi:
        return true;
    default:
        return false;
    }
}

}

bool FunctionEmitter::AnalyzeBytecode() {
    bytecode_ = function_->GetByteCode(&bytecodeLength_);
    if (!bytecode_ || bytecodeLength_ == 0) return false;
    engine_ = static_cast<asCScriptEngine*>(function_->GetEngine());
    scriptFunction_ = static_cast<asCScriptFunction*>(function_);

    if (!DecodeInstructions()) return false;
    needsLabel_.assign(instructions_.size(), 0);
    localCatchTarget_.assign(instructions_.size(), -1);
    refCopyFusionSpan_.assign(instructions_.size(), 0);
    refCopyFusionSkip_.assign(instructions_.size(), 0);
    fusedCmpBranch_.assign(instructions_.size(), 0);
    fusedFallValue_.assign(instructions_.size(), 2);

    if (!AnalyzeLabels() || !AnalyzeCatchTargets()) return false;
    AnalyzeReferenceCopyFusions();
    return AnalyzeComparisonBranchFusions();
}

bool FunctionEmitter::DecodeInstructions() {
    instructions_.reserve(bytecodeLength_);
    indexOfOffset_.assign(bytecodeLength_, -1);
    uint32_t offset = 0;
    while (offset < bytecodeLength_) {
        asEBCInstr op =
            static_cast<asEBCInstr>(bytecode_[offset] & 0xFF);
        const int size = BcSize(op);
        if (size <= 0) return false;
        indexOfOffset_[offset] = static_cast<int>(instructions_.size());
        instructions_.push_back(
            Instruction{op, offset, static_cast<uint32_t>(size)});
        offset += static_cast<uint32_t>(size);
    }
    if (offset != bytecodeLength_) return false;
    inlineFieldMemory_ = instructions_.size() <= 256;
    bool hasDoubleArithmetic = false;
    cacheLocals_ = scriptFunction_->scriptData &&
                   scriptFunction_->scriptData->variableSpace <= 64;
    for (const Instruction& instruction : instructions_) {
        cacheLocals_ = cacheLocals_ && IsCacheableLocalOp(instruction.op);
        hasDoubleArithmetic = hasDoubleArithmetic ||
            instruction.op == asBC_ADDd || instruction.op == asBC_SUBd ||
            instruction.op == asBC_MULd || instruction.op == asBC_DIVd;
    }
    cacheLocals_ = cacheLocals_ && hasDoubleArithmetic;
    return true;
}

bool FunctionEmitter::AnalyzeLabels() {
    for (size_t i = 0; i < instructions_.size(); i++) {
        const Instruction& instruction = instructions_[i];
        const asDWORD* ip = bytecode_ + instruction.off;
        if (instruction.op == asBC_JitEntry) {
            needsLabel_[i] = 1;
            continue;
        }
        if (instruction.op != asBC_JMP &&
            !IsConditionalBranch(instruction.op))
            continue;
        const int targetIndex = BranchTargetIndex(instruction, ip);
        if (targetIndex < 0) return false;
        needsLabel_[static_cast<size_t>(targetIndex)] = 1;
    }
    return true;
}

bool FunctionEmitter::AnalyzeCatchTargets() {
    if (!scriptFunction_->scriptData) return true;
    for (size_t i = 0; i < instructions_.size(); i++) {
        int catchTarget = -1;
        for (asUINT tryIndex = 0;
             tryIndex < scriptFunction_->scriptData->tryCatchInfo.GetLength();
             tryIndex++) {
            const asSTryCatchInfo& info =
                scriptFunction_->scriptData->tryCatchInfo[tryIndex];
            if (instructions_[i].off >= info.tryPos &&
                instructions_[i].off < info.catchPos) {
                if (info.catchPos >= bytecodeLength_) return false;
                catchTarget = indexOfOffset_[info.catchPos];
            }
        }
        if (catchTarget >= 0) {
            localCatchTarget_[i] = catchTarget;
            needsLabel_[static_cast<size_t>(catchTarget)] = 1;
        }
    }
    return true;
}

void FunctionEmitter::AnalyzeReferenceCopyFusions() {
    for (size_t i = 0; i + 2 < instructions_.size(); i++) {
        if (instructions_[i].op != asBC_PshVPtr ||
            instructions_[i + 1].op != asBC_RefCpyV ||
            needsLabel_[i + 1])
            continue;

        const asDWORD* copy = bytecode_ + instructions_[i + 1].off;
        auto* copyType = reinterpret_cast<asCObjectType*>(asBC_PTRARG(copy));
        if (!(copyType->flags & (asOBJ_SCRIPT_OBJECT | asOBJ_FUNCDEF)))
            continue;

        unsigned span = 0;
        if (instructions_[i + 2].op == asBC_PopPtr &&
            !needsLabel_[i + 2]) {
            span = 3;
        } else if (i + 3 < instructions_.size() &&
                   instructions_[i + 2].op == asBC_FREE &&
                   instructions_[i + 3].op == asBC_PopPtr &&
                   !needsLabel_[i + 2] && !needsLabel_[i + 3]) {
            const asDWORD* push = bytecode_ + instructions_[i].off;
            const asDWORD* release = bytecode_ + instructions_[i + 2].off;
            if (asBC_SWORDARG0(push) != asBC_SWORDARG0(release) ||
                asBC_SWORDARG0(push) == asBC_SWORDARG0(copy) ||
                asBC_PTRARG(copy) != asBC_PTRARG(release))
                continue;
            span = 4;
        }
        if (!span) continue;
        refCopyFusionSpan_[i] = static_cast<uint8_t>(span);
        for (unsigned skipped = 1; skipped < span; skipped++)
            refCopyFusionSkip_[i + skipped] = 1;
        i += span - 1;
    }
}

bool FunctionEmitter::IsValueRegisterDeadFrom(size_t start) const {
    std::vector<uint8_t> visited(instructions_.size(), 0);
    size_t current = start;
    while (current < instructions_.size()) {
        if (visited[current]) return false;
        visited[current] = 1;
        const Instruction& instruction = instructions_[current];
        if (WritesValueRegister(instruction.op)) return true;
        if (instruction.op == asBC_JMP) {
            const int targetIndex = BranchTargetIndex(
                instruction, bytecode_ + instruction.off);
            if (targetIndex < 0) return false;
            current = static_cast<size_t>(targetIndex);
            continue;
        }
        if (!PreservesValueRegister(instruction.op)) return false;
        current++;
    }
    return false;
}

bool FunctionEmitter::AnalyzeComparisonBranchFusions() {
    constexpr bool kFuseCmpBranch = true;
    constexpr bool kInlineCmp6c = true;
    if (!kFuseCmpBranch || !kInlineCmp6c) return true;

    for (size_t i = 0; i + 2 < instructions_.size(); i++) {
        if ((instructions_[i].op != asBC_CMPi &&
             instructions_[i].op != asBC_CMPIi &&
             instructions_[i].op != asBC_CmpPtr) ||
            !IsConditionalBranch(instructions_[i + 1].op) ||
            needsLabel_[i + 1])
            continue;
        const Instruction& branch = instructions_[i + 1];
        const int targetIndex =
            BranchTargetIndex(branch, bytecode_ + branch.off);
        if (targetIndex < 0) return false;
        const bool fallDead = IsValueRegisterDeadFrom(i + 2);
        const bool takenDead =
            IsValueRegisterDeadFrom(static_cast<size_t>(targetIndex));
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
            fusedCmpBranch_[i] = 1;
            if (!fallDead)
                fusedFallValue_[i] = static_cast<int8_t>(fallValue);
        }
    }
    return true;
}

}
