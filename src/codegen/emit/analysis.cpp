#include "codegen/emit/emitter.h"

#include "bytecode/bc_info.h"

#include <algorithm>

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
    case asBC_CMPu:
    case asBC_CMPIu:
    case asBC_CMPf:
    case asBC_CMPd:
    case asBC_CMPIf:
    case asBC_TZ:
    case asBC_TNZ:
    case asBC_TS:
    case asBC_TNS:
    case asBC_TP:
    case asBC_TNP:
    case asBC_JMPP:
    case asBC_POWi:
    case asBC_DIVu:
    case asBC_MODu:
    case asBC_ADDi64:
    case asBC_SUBi64:
    case asBC_MULi64:
    case asBC_DIVi64:
    case asBC_MODi64:
    case asBC_NEGi64:
    case asBC_BNOT64:
    case asBC_BAND64:
    case asBC_BOR64:
    case asBC_BXOR64:
    case asBC_uTOi64:
    // Ops below never access frame locals directly in their emitters
    // (calls and object opcodes go through LoadVar/StoreVar or C++
    // helpers whose boundaries flush the local cache), so functions
    // containing them can still keep locals in registers.
    case asBC_CALL:
    case asBC_CALLSYS:
    case asBC_CallPtr:
    case asBC_CALLINTF:
    case asBC_CALLBND:
    case asBC_ALLOC:
    case asBC_FREE:
    case asBC_STOREOBJ:
    case asBC_LOADOBJ:
    case asBC_REFCPY:
    case asBC_COPY:
    case asBC_CHKREF:
    case asBC_ChkRefS:
    case asBC_ChkNullS:
    case asBC_Cast:
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

namespace {

// Branchless opcodes that read the 32-bit value register. Window consumers
// must be branchless so the deferred memory write lands on every path.
bool ReadsValueRegister32(asEBCInstr op) {
    switch (op) {
    case asBC_CpyRtoV4:
    case asBC_RDR1:
    case asBC_RDR2:
    case asBC_RDR4:
    case asBC_RDR8:
    case asBC_WRTV1:
    case asBC_WRTV2:
    case asBC_WRTV4:
    case asBC_WRTV8:
    case asBC_INCi:
    case asBC_DECi:
    case asBC_INCf:
    case asBC_DECf:
    case asBC_INCd:
    case asBC_DECd:
    case asBC_INCi16:
    case asBC_DECi16:
    case asBC_INCi8:
    case asBC_DECi8:
    case asBC_PshRPtr:
        return true;
    default:
        return false;
    }
}

// Opcodes whose emission writes the value register through the forwarding
// helper, so a following consumer can reuse the value without the memory
// round trip.
bool StartsValueRegisterForward(asEBCInstr op) {
    switch (op) {
    case asBC_CpyVtoR4:
    case asBC_LDV:
    case asBC_LoadVObjR:
    case asBC_LoadThisR:
    case asBC_LoadRObjR:
    case asBC_PopRPtr:
        return true;
    default:
        return false;
    }
}

}  // namespace

void FunctionEmitter::AnalyzeValueRegisterForwards() {
    vrForwardSpan_.assign(instructions_.size(), 0);
    vrForwardConsume_.assign(instructions_.size(), 0);
    vrForwardDead_.assign(instructions_.size(), 0);
    if (!inlineFieldMemory_) return;
    for (size_t i = 0; i + 1 < instructions_.size(); i++) {
        if (!StartsValueRegisterForward(instructions_[i].op)) continue;
        size_t j = i + 1;
        while (j < instructions_.size()) {
            if (needsLabel_[j] || refCopyFusionSkip_[j]) break;
            if (j > 0 && fusedCmpBranch_[j - 1]) break;
            if (j > 1 && fusedCmpBranch_[j - 2] == 2) break;
            const asEBCInstr op = instructions_[j].op;
            if (ReadsValueRegister32(op)) {
                vrForwardSpan_[i] = static_cast<uint8_t>(j - i);
                vrForwardConsume_[j] = 1;
                vrForwardDead_[j] = IsValueRegisterDeadFrom(j + 1) ? 1 : 0;
                break;
            }
            if (WritesValueRegister(op)) break;
            if (!PreservesValueRegister(op)) break;
            j++;
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
    localCatchInfo_.assign(instructions_.size(), nullptr);
    localCatchNeedsCleanup_.assign(instructions_.size(), 1);
    refCopyFusionSpan_.assign(instructions_.size(), 0);
    refCopyFusionSkip_.assign(instructions_.size(), 0);
    fusedCmpBranch_.assign(instructions_.size(), 0);
    fusedInvertBranch_.assign(instructions_.size(), 0);
    fusedFallValue_.assign(instructions_.size(), 2);

    if (!AnalyzeLabels() || !AnalyzeCatchTargets()) return false;
    AnalyzeReferenceCopyFusions();
    if (!AnalyzeComparisonBranchFusions()) return false;
    AnalyzeValueRegisterForwards();
    return true;
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
    cacheLocals_ = scriptFunction_->scriptData != nullptr;
    unsigned floatOpCount = 0;
    unsigned callBoundaryCount = 0;
    unsigned branchCount = 0;
    for (const Instruction& instruction : instructions_) {
        cacheLocals_ = cacheLocals_ && IsCacheableLocalOp(instruction.op);
        if (IsConditionalBranch(instruction.op) || instruction.op == asBC_JMP ||
            instruction.op == asBC_JMPP)
            branchCount++;
        switch (instruction.op) {
        case asBC_ADDf:
        case asBC_SUBf:
        case asBC_MULf:
        case asBC_ADDIf:
        case asBC_SUBIf:
        case asBC_MULIf:
            floatOpCount++;
            break;
        case asBC_CALL:
        case asBC_CALLSYS:
        case asBC_CallPtr:
        case asBC_CALLINTF:
        case asBC_CALLBND:
        case asBC_ALLOC:
            callBoundaryCount++;
            break;
        default:
            break;
        }
    }
    // Float arithmetic runs faster through the packed SSE windows, which
    // are disabled under local caching; call-heavy bodies pay a
    // flush/reload per boundary; branchy or oversized code falls back to
    // helpers often enough that the per-call flush/reload dominates; tiny
    // bodies never amortize the entry loads and exit flush.
    cacheLocals_ = cacheLocals_ && inlineFieldMemory_ &&
                   instructions_.size() >= 16 && floatOpCount < 2 &&
                   callBoundaryCount <= 3 &&
                   branchCount * 7 < instructions_.size();
    if (cacheLocals_) CollectCachedLocalOffsets();
    return true;
}

void FunctionEmitter::CollectCachedLocalOffsets() {
    // Only locals referenced by word operands of cacheable opcodes get a
    // register; extra offsets would just consume registers, and a missing
    // one only falls back to the frame slot.
    cachedLocalOffsets_.clear();
    const auto record = [&](int offset) {
        if (offset <= 0) return;
        if (std::find(cachedLocalOffsets_.begin(),
                      cachedLocalOffsets_.end(),
                      offset) == cachedLocalOffsets_.end())
            cachedLocalOffsets_.push_back(offset);
    };
    for (const Instruction& instruction : instructions_) {
        if (!IsCacheableLocalOp(instruction.op)) continue;
        const asDWORD* ip = bytecode_ + instruction.off;
        record(asBC_SWORDARG0(ip));
        record(asBC_SWORDARG1(ip));
        record(asBC_SWORDARG2(ip));
    }
    // 64-bit locals occupy slots (n, n-1); a half-cached pair would let the
    // qword memory access in LoadVar64/StoreVar64 race the cached half.
    for (size_t i = 0; i < cachedLocalOffsets_.size(); i++)
        record(cachedLocalOffsets_[i] - 1);
    std::sort(cachedLocalOffsets_.begin(), cachedLocalOffsets_.end());
    // Bound register pressure; prefer the lowest offsets (temporaries and
    // the hottest script locals live there).
    if (cachedLocalOffsets_.size() > kMaxCachedLocals) {
        cachedLocalOffsets_.resize(kMaxCachedLocals);
        cacheLocals_ = cachedLocalOffsets_.size() >= 4;
    }
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
        const asSTryCatchInfo* catchInfo = nullptr;
        for (asUINT tryIndex = 0;
             tryIndex < scriptFunction_->scriptData->tryCatchInfo.GetLength();
             tryIndex++) {
            const asSTryCatchInfo& info =
                scriptFunction_->scriptData->tryCatchInfo[tryIndex];
            if (instructions_[i].off >= info.tryPos &&
                instructions_[i].off < info.catchPos) {
                if (info.catchPos >= bytecodeLength_) return false;
                catchTarget = indexOfOffset_[info.catchPos];
                catchInfo = &info;
            }
        }
        if (catchTarget >= 0) {
            localCatchTarget_[i] = catchTarget;
            localCatchInfo_[i] = catchInfo;
            bool needsCleanup = false;
            for (asUINT objectIndex = 0;
                 objectIndex <
                     scriptFunction_->scriptData->objVariableInfo.GetLength();
                 objectIndex++) {
                const asSObjectVariableInfo& objectInfo =
                    scriptFunction_->scriptData->objVariableInfo[objectIndex];
                if (objectInfo.programPos >= catchInfo->tryPos &&
                    objectInfo.programPos < catchInfo->catchPos &&
                    (objectInfo.option == asOBJ_INIT ||
                     objectInfo.option == asOBJ_VARDECL)) {
                    needsCleanup = true;
                    break;
                }
            }
            localCatchNeedsCleanup_[i] = needsCleanup ? 1 : 0;
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
                   ((instructions_[i + 2].op == asBC_FREE &&
                     instructions_[i + 3].op == asBC_PopPtr) ||
                    (instructions_[i + 2].op == asBC_PopPtr &&
                     instructions_[i + 3].op == asBC_FREE)) &&
                   !needsLabel_[i + 2] && !needsLabel_[i + 3]) {
            const asDWORD* push = bytecode_ + instructions_[i].off;
            const size_t releaseIndex =
                instructions_[i + 2].op == asBC_FREE ? i + 2 : i + 3;
            const asDWORD* release =
                bytecode_ + instructions_[releaseIndex].off;
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
        if (instructions_[i].op != asBC_CMPi &&
            instructions_[i].op != asBC_CMPIi &&
            instructions_[i].op != asBC_CMPu &&
            instructions_[i].op != asBC_CMPIu &&
            instructions_[i].op != asBC_CmpPtr)
            continue;

        size_t branchIndex = i + 1;
        bool invert = false;
        if ((instructions_[i + 1].op == asBC_TZ ||
             instructions_[i + 1].op == asBC_TNZ) &&
            i + 3 <= instructions_.size() &&
            !needsLabel_[i + 1] && !needsLabel_[i + 2] &&
            IsConditionalBranch(instructions_[i + 2].op) &&
            (instructions_[i + 2].op == asBC_JZ ||
             instructions_[i + 2].op == asBC_JNZ ||
             instructions_[i + 2].op == asBC_JLowZ ||
             instructions_[i + 2].op == asBC_JLowNZ)) {
            invert = instructions_[i + 1].op == asBC_TZ;
            branchIndex = i + 2;
        } else if (!IsConditionalBranch(instructions_[i + 1].op) ||
                   needsLabel_[i + 1]) {
            continue;
        }

        const Instruction& branch = instructions_[branchIndex];
        const int targetIndex =
            BranchTargetIndex(branch, bytecode_ + branch.off);
        if (targetIndex < 0) return false;
        const bool fallDead = IsValueRegisterDeadFrom(branchIndex + 1);
        const bool takenDead =
            IsValueRegisterDeadFrom(static_cast<size_t>(targetIndex));
        asEBCInstr branchOp = branch.op;
        if (invert) {
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
        int fallValue = 2;
        switch (branchOp) {
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
            fusedCmpBranch_[i] =
                static_cast<uint8_t>(branchIndex - i);
            fusedInvertBranch_[i] = invert ? 1 : 0;
            if (!fallDead)
                fusedFallValue_[i] = static_cast<int8_t>(fallValue);
        }
    }
    return true;
}

}
