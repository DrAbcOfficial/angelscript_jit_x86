#include "bytecode/helpers/runtime_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcCall(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = NextBc(bc, 2);
    ctx->CallScriptFunction(ctx->m_engine->scriptFunctions[i]);
    return JITBC_EXIT;
}

int BcRet(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    if (ctx->m_callStack.GetLength() == 0 ||
        ctx->m_callStack[ctx->m_callStack.GetLength() - CALLSTACK_FRAME_SIZE] == 0) {
        ctx->m_status = asEXECUTION_FINISHED;
        return JITBC_EXIT;
    }
    asWORD w = asBC_WORDARG0(bc);
    ctx->PopCallState();
    regs->stackPointer += w;
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
    }
    else if (func->funcType == asFUNC_SYSTEM) {
        regs->stackPointer += CallSystemFunction(func->id, ctx);
        regs->programPointer += 2;
    }
    else {
        assert(func->funcType == asFUNC_DELEGATE);
    }
    return JITBC_EXIT;
}

int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = NextBc(bc, 2);
    ctx->CallInterfaceMethod(ctx->m_engine->GetScriptFunction(i));
    return JITBC_EXIT;
}

int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCScriptFunction* func = *(asCScriptFunction**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bool systemCall = false;
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
        if (funcId > 0)
            ctx->CallScriptFunction(ctx->m_engine->scriptFunctions[funcId]);
        else {
            ctx->m_needToCleanupArgs = true;
            ctx->SetInternalException(TXT_UNBOUND_FUNCTION);
        }
    }
    else {
        assert(false);
    }
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
