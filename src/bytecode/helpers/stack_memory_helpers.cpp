#include "bytecode/helpers/stack_memory_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcPopPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer += AS_PTR_SIZE;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPshGPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcPshC4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = asBC_DWORDARG(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPshV4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPSF(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asPWORD(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcSwapPtr(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD p = *(asPWORD*)regs->stackPointer;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)(regs->stackPointer + AS_PTR_SIZE);
    *(asPWORD*)(regs->stackPointer + AS_PTR_SIZE) = p;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPshG4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = *(asDWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcLdGRdR4(asSVMRegisters* regs, const asDWORD* bc) {
    *(void**)&regs->valueRegister = (void*)asBC_PTRARG(bc);
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asDWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcCopy(asSVMRegisters* regs, const asDWORD* bc) {
    void* d = (void*)*(asPWORD*)regs->stackPointer; regs->stackPointer += AS_PTR_SIZE;
    void* s = (void*)*(asPWORD*)regs->stackPointer;
    if (s == 0 || d == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    memcpy(d, s, asBC_WORDARG0(bc) * 4);
    *(asPWORD**)regs->stackPointer = (asPWORD*)d;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPshC8(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= 2;
    *(asQWORD*)regs->stackPointer = asBC_QWORDARG(bc);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcPshVPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRdsPtr(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD a = *(asPWORD*)regs->stackPointer;
    if (a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    *(asPWORD*)regs->stackPointer = *(asPWORD*)a;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPopRPtr(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)&regs->valueRegister = *(asPWORD*)regs->stackPointer;
    regs->stackPointer += AS_PTR_SIZE;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPshRPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcStr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcSetV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asBC_DWORDARG(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSetV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asBC_QWORDARG(bc);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcAddSi(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD a = *(asPWORD*)regs->stackPointer;
    if (a == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_NULL_POINTER_ACCESS);
        return JITBC_EXIT;
    }
    *(asPWORD*)regs->stackPointer = a + asBC_SWORDARG0(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCpyVtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCpyVtoV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCpyVtoR4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)&regs->valueRegister = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcCpyVtoR8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)&regs->valueRegister = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcCpyVtoG4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)asBC_PTRARG(bc) = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcCpyRtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asDWORD*)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcCpyRtoV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcCpyGtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asDWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcWrtV1(asSVMRegisters* regs, const asDWORD* bc) {
    **(asBYTE**)&regs->valueRegister = *(asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcWrtV2(asSVMRegisters* regs, const asDWORD* bc) {
    **(asWORD**)&regs->valueRegister = *(asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcWrtV4(asSVMRegisters* regs, const asDWORD* bc) {
    **(asDWORD**)&regs->valueRegister = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcWrtV8(asSVMRegisters* regs, const asDWORD* bc) {
    **(asQWORD**)&regs->valueRegister = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRdr1(asSVMRegisters* regs, const asDWORD* bc) {
    asBYTE* bPtr = (asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bPtr[0] = **(asBYTE**)&regs->valueRegister;
    bPtr[1] = 0;
    bPtr[2] = 0;
    bPtr[3] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRdr2(asSVMRegisters* regs, const asDWORD* bc) {
    asWORD* wPtr = (asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    wPtr[0] = **(asWORD**)&regs->valueRegister;
    wPtr[1] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRdr4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asDWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcRdr8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asQWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcLdg(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)&regs->valueRegister = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcLdv(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD**)&regs->valueRegister = (regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPga(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcVar(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = (asPWORD)asBC_SWORDARG0(bc);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcPshV8(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= 2;
    *(asQWORD*)regs->stackPointer = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

}
