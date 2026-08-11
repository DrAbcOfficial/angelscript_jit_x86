#include "bytecode/helpers/runtime_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_objecttype.h"
#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

namespace {

void PrepareScriptCall(asCContext* ctx, asCScriptFunction* function) {
    assert(function->scriptData);
    if (ctx->PushCallState() < 0) return;

    ctx->m_currentFunction = function;
    ctx->m_regs.programPointer = function->scriptData->byteCode.AddressOf();

    asDWORD* oldStackPointer = ctx->m_regs.stackPointer;
    if (!ctx->ReserveStackSpace(function->scriptData->stackNeeded)) return;

    if (ctx->m_regs.stackPointer != oldStackPointer) {
        int argumentDwords = function->GetSpaceNeededForArguments() +
                            (function->objectType ? AS_PTR_SIZE : 0) +
                            (function->DoesReturnOnStack() ? AS_PTR_SIZE : 0);
        std::memcpy(ctx->m_regs.stackPointer, oldStackPointer,
                    sizeof(asDWORD) * argumentDwords);
    }

    ctx->m_regs.stackFramePointer = ctx->m_regs.stackPointer;
    for (asUINT index = function->scriptData->variables.GetLength(); index-- > 0;) {
        asSScriptVariable* variable = function->scriptData->variables[index];
        if (variable->stackOffset <= 0) continue;
        if (variable->onHeap &&
            (variable->type.IsObject() || variable->type.IsFuncdef())) {
            *reinterpret_cast<asPWORD*>(
                &ctx->m_regs.stackFramePointer[-variable->stackOffset]) = 0;
        }
    }

    ctx->m_regs.stackPointer -= function->scriptData->variableSpace;
    if (ctx->m_regs.doProcessSuspend) {
        if (ctx->m_lineCallback) ctx->CallLineCallback();
        if (ctx->m_doSuspend) ctx->m_status = asEXECUTION_SUSPENDED;
    }
}

asCScriptFunction* ResolveScriptMethod(asCScriptObject* object,
                                       asCScriptFunction* function) {
    auto* objectType = static_cast<asCObjectType*>(object->GetObjectType());
    if (function->funcType == asFUNC_VIRTUAL)
        return objectType->virtualFunctionTable[function->vfTableIdx];

    assert(function->funcType == asFUNC_INTERFACE);
    asUINT interfaceOffset = 0;
    for (asUINT index = 0; index < objectType->interfaces.GetLength(); index++) {
        if (objectType->interfaces[index] == function->objectType) {
            interfaceOffset = objectType->interfaceVFTOffsets[index];
            return objectType->virtualFunctionTable[
                function->vfTableIdx + interfaceOffset];
        }
    }
    return nullptr;
}

}

int ResumeJitCallChain(asSVMRegisters* regs, asUINT callerCallStackLength,
                       unsigned maxDirectDepth) {
    auto* ctx = Ctx(regs);
    if (callerCallStackLength / CALLSTACK_FRAME_SIZE >= maxDirectDepth)
        return JITBC_EXIT;

    while (ctx->m_status == asEXECUTION_ACTIVE &&
           ctx->m_callStack.GetLength() > callerCallStackLength) {
        asCScriptFunction* function = ctx->m_currentFunction;
        asDWORD* programPointer = regs->programPointer;
        if (!function || !function->scriptData || !function->scriptData->jitFunction ||
            !programPointer || (*programPointer & 0xFF) != asBC_JitEntry)
            return JITBC_EXIT;

        asPWORD jitArg = asBC_PTRARG(programPointer);
        if (!jitArg) return JITBC_EXIT;
        function->scriptData->jitFunction(regs, jitArg);
    }

    return ctx->m_status == asEXECUTION_ACTIVE &&
           ctx->m_callStack.GetLength() == callerCallStackLength ?
           JITBC_CONTINUE : JITBC_EXIT;
}

int CallScriptFunction(asSVMRegisters* regs, asCScriptFunction* function,
                       const asDWORD* nextBc) {
    auto* ctx = Ctx(regs);
    asUINT callerCallStackLength = ctx->m_callStack.GetLength();
    regs->programPointer = const_cast<asDWORD*>(nextBc);
    PrepareScriptCall(ctx, function);
    return ResumeJitCallChain(regs, callerCallStackLength);
}

int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcCall(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    return CallScriptFunction(regs,
                              ctx->m_engine->scriptFunctions[asBC_INTARG(bc)],
                              NextBc(bc, 2));
}

int BcRet(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    if (ctx->m_callStack.GetLength() == 0 ||
        ctx->m_callStack[ctx->m_callStack.GetLength() - CALLSTACK_FRAME_SIZE] == 0) {
        ctx->m_status = asEXECUTION_FINISHED;
        return JITBC_EXIT;
    }
    asWORD popSize = asBC_WORDARG0(bc);
    ctx->PopCallState();
    regs->stackPointer += popSize;
    return JITBC_EXIT;
}

int BcCallSys(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = const_cast<asDWORD*>(bc);
    regs->stackPointer += CallSystemFunction(i, ctx);
    regs->programPointer = NextBc(bc, 2);
    if (regs->doProcessSuspend) {
        if (ctx->m_doSuspend) {
            ctx->m_status = asEXECUTION_SUSPENDED;
            return JITBC_EXIT;
        }
        if (ctx->m_status != asEXECUTION_ACTIVE)
            return JITBC_EXIT;
    }
    return JITBC_CONTINUE;
}

int BcCallBnd(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asUINT callerCallStackLength = ctx->m_callStack.GetLength();
    int i = asBC_INTARG(bc);
    regs->programPointer = const_cast<asDWORD*>(bc);
    int funcId = ctx->m_engine->importedFunctions[i & ~FUNC_IMPORTED]->boundFunctionId;
    if (funcId == -1) {
        regs->programPointer += 2;
        ctx->m_needToCleanupArgs = true;
        ctx->SetInternalException(TXT_UNBOUND_FUNCTION);
        return JITBC_EXIT;
    }
    asCScriptFunction* func = ctx->m_engine->GetScriptFunction(funcId);
    if (func->funcType == asFUNC_SCRIPT) {
        regs->programPointer += 2;
        ctx->CallScriptFunction(func);
        return ResumeJitCallChain(regs, callerCallStackLength);
    }
    else if (func->funcType == asFUNC_SYSTEM) {
        regs->stackPointer += CallSystemFunction(func->id, ctx);
        regs->programPointer += 2;
        return ctx->m_status == asEXECUTION_ACTIVE ? JITBC_CONTINUE : JITBC_EXIT;
    }
    else {
        assert(func->funcType == asFUNC_DELEGATE);
    }
    return JITBC_EXIT;
}

int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    const asDWORD* nextBc = NextBc(bc, 2);
    auto* object = *reinterpret_cast<asCScriptObject**>(regs->stackPointer);
    if (!object) {
        regs->programPointer = const_cast<asDWORD*>(nextBc);
        ctx->m_needToCleanupArgs = true;
        ctx->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }

    auto* function = ctx->m_engine->GetScriptFunction(asBC_INTARG(bc));
    asCScriptFunction* realFunction = ResolveScriptMethod(object, function);
    if (!realFunction) {
        regs->programPointer = const_cast<asDWORD*>(nextBc);
        ctx->m_needToCleanupArgs = true;
        ctx->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    return CallScriptFunction(regs, realFunction, nextBc);
}

int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asUINT callerCallStackLength = ctx->m_callStack.GetLength();
    asCScriptFunction* func = *(asCScriptFunction**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bool systemCall = false;
    bool scriptCall = false;
    regs->programPointer = const_cast<asDWORD*>(bc);
    if (func == 0) {
        regs->programPointer++;
        ctx->m_needToCleanupArgs = true;
        ctx->SetInternalException(TXT_UNBOUND_FUNCTION);
        return JITBC_EXIT;
    }
    if (func->funcType == asFUNC_SCRIPT) {
        regs->programPointer++;
        ctx->CallScriptFunction(func);
        scriptCall = true;
    }
    else if (func->funcType == asFUNC_DELEGATE) {
        regs->stackPointer -= AS_PTR_SIZE;
        *(asPWORD*)regs->stackPointer = asPWORD(func->objForDelegate);
        if (func->funcForDelegate->funcType == asFUNC_SYSTEM) {
            regs->stackPointer += CallSystemFunction(func->funcForDelegate->id, ctx);
            regs->programPointer++;
            systemCall = true;
        }
        else {
            regs->programPointer++;
            ctx->CallInterfaceMethod(func->funcForDelegate);
            scriptCall = true;
        }
    }
    else if (func->funcType == asFUNC_SYSTEM) {
        regs->stackPointer += CallSystemFunction(func->id, ctx);
        regs->programPointer++;
        systemCall = true;
    }
    else if (func->funcType == asFUNC_IMPORTED) {
        regs->programPointer++;
        int funcId = ctx->m_engine->importedFunctions[func->id & ~FUNC_IMPORTED]->boundFunctionId;
        if (funcId > 0) {
            ctx->CallScriptFunction(ctx->m_engine->scriptFunctions[funcId]);
            scriptCall = true;
        } else {
            ctx->m_needToCleanupArgs = true;
            ctx->SetInternalException(TXT_UNBOUND_FUNCTION);
        }
    }
    else {
        assert(false);
    }
    if (scriptCall)
        return ResumeJitCallChain(regs, callerCallStackLength);
    return systemCall && ctx->m_status == asEXECUTION_ACTIVE ? JITBC_CONTINUE : JITBC_EXIT;
}

int BcThiscall1(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = const_cast<asDWORD*>(bc);
    void* obj = *(void**)regs->stackPointer;
    if (obj == 0) {
        ctx->SetInternalException(TXT_NULL_POINTER_ACCESS);
        regs->programPointer = NextBc(bc, 2);
        return JITBC_EXIT;
    }
    regs->stackPointer += AS_PTR_SIZE;
    int arg = *(int*)regs->stackPointer;
    regs->stackPointer++;
    ctx->m_callingSystemFunction = ctx->m_engine->scriptFunctions[i];
    void* ptr = 0;
#ifdef AS_NO_EXCEPTIONS
    ptr = ctx->m_engine->CallObjectMethodRetPtr(obj, arg, ctx->m_callingSystemFunction);
#else
    try {
        ptr = ctx->m_engine->CallObjectMethodRetPtr(obj, arg, ctx->m_callingSystemFunction);
    }
    catch (...) {
        ctx->HandleAppException();
    }
#endif
    ctx->m_callingSystemFunction = 0;
    *(asPWORD*)&regs->valueRegister = (asPWORD)ptr;
    regs->programPointer = NextBc(bc, 2);
    if (regs->doProcessSuspend) {
        if (ctx->m_doSuspend) {
            ctx->m_status = asEXECUTION_SUSPENDED;
            return JITBC_EXIT;
        }
        if (ctx->m_status != asEXECUTION_ACTIVE)
            return JITBC_EXIT;
    }
    return JITBC_CONTINUE;
}

int BcJmpP(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = const_cast<asDWORD*>(bc + 1 + (*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc))) * 2);
    return JITBC_EXIT;
}

int BcSuspend(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    if (regs->doProcessSuspend) {
        if (ctx->m_lineCallback) {
            regs->programPointer = const_cast<asDWORD*>(bc);
            ctx->CallLineCallback();
        }
        if (ctx->m_doSuspend) {
            regs->programPointer = NextBc(bc, 1);
            ctx->m_status = asEXECUTION_SUSPENDED;
            return JITBC_EXIT;
        }
    }
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

}
