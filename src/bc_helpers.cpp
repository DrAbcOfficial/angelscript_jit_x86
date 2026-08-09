#include "bc_helpers.h"
#include "bc_info.h"

#include "as_context.h"
#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86 {

namespace {

inline asCContext* Ctx(asSVMRegisters* regs) {
    return static_cast<asCContext*>(regs->ctx);
}

inline asDWORD* NextBc(const asDWORD* bc, int n) {
    return const_cast<asDWORD*>(bc + n);
}

static int BcPopPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer += AS_PTR_SIZE;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPshGPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcPshC4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = asBC_DWORDARG(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcPshV4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPSF(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asPWORD(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcSwapPtr(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD p = *(asPWORD*)regs->stackPointer;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)(regs->stackPointer + AS_PTR_SIZE);
    *(asPWORD*)(regs->stackPointer + AS_PTR_SIZE) = p;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcNot(asSVMRegisters* regs, const asDWORD* bc) {
#if AS_SIZEOF_BOOL == 1
    volatile asBYTE* ptr = (asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asBYTE val = (ptr[0] == 0) ? VALUE_OF_BOOLEAN_TRUE : 0;
    ptr[0] = val;
    ptr[1] = 0;
    ptr[2] = 0;
    ptr[3] = 0;
#else
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = (*(regs->stackFramePointer - asBC_SWORDARG0(bc)) == 0 ? VALUE_OF_BOOLEAN_TRUE : 0);
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPshG4(asSVMRegisters* regs, const asDWORD* bc) {
    --regs->stackPointer;
    *regs->stackPointer = *(asDWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcLdGRdR4(asSVMRegisters* regs, const asDWORD* bc) {
    *(void**)&regs->valueRegister = (void*)asBC_PTRARG(bc);
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asDWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcTestZero(asSVMRegisters* regs, const asDWORD* bc, bool zero) {
#if AS_SIZEOF_BOOL == 1
    volatile int* regPtr = (int*)&regs->valueRegister;
    volatile asBYTE* regBptr = (asBYTE*)&regs->valueRegister;
    asBYTE val = ((regPtr[0] == 0) == zero) ? VALUE_OF_BOOLEAN_TRUE : 0;
    regBptr[0] = val;
    regBptr[1] = 0;
    regBptr[2] = 0;
    regBptr[3] = 0;
    regBptr[4] = 0;
    regBptr[5] = 0;
    regBptr[6] = 0;
    regBptr[7] = 0;
#else
    *(int*)&regs->valueRegister = ((*(int*)&regs->valueRegister == 0) == zero) ? VALUE_OF_BOOLEAN_TRUE : 0;
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcTestSign(asSVMRegisters* regs, const asDWORD* bc, bool neg) {
#if AS_SIZEOF_BOOL == 1
    volatile int* regPtr = (int*)&regs->valueRegister;
    volatile asBYTE* regBptr = (asBYTE*)&regs->valueRegister;
    asBYTE val = ((regPtr[0] < 0) == neg) ? VALUE_OF_BOOLEAN_TRUE : 0;
    regBptr[0] = val;
    regBptr[1] = 0;
    regBptr[2] = 0;
    regBptr[3] = 0;
    regBptr[4] = 0;
    regBptr[5] = 0;
    regBptr[6] = 0;
    regBptr[7] = 0;
#else
    *(int*)&regs->valueRegister = ((*(int*)&regs->valueRegister < 0) == neg) ? VALUE_OF_BOOLEAN_TRUE : 0;
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcTestPos(asSVMRegisters* regs, const asDWORD* bc, bool pos) {
#if AS_SIZEOF_BOOL == 1
    volatile int* regPtr = (int*)&regs->valueRegister;
    volatile asBYTE* regBptr = (asBYTE*)&regs->valueRegister;
    asBYTE val = ((regPtr[0] > 0) == pos) ? VALUE_OF_BOOLEAN_TRUE : 0;
    regBptr[0] = val;
    regBptr[1] = 0;
    regBptr[2] = 0;
    regBptr[3] = 0;
    regBptr[4] = 0;
    regBptr[5] = 0;
    regBptr[6] = 0;
    regBptr[7] = 0;
#else
    *(int*)&regs->valueRegister = ((*(int*)&regs->valueRegister > 0) == pos) ? VALUE_OF_BOOLEAN_TRUE : 0;
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcNegi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asDWORD(-int(*(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcNegf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcNegd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcInci16(asSVMRegisters* regs, const asDWORD* bc) {
    (**(short**)&regs->valueRegister)++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcInci8(asSVMRegisters* regs, const asDWORD* bc) {
    (**(char**)&regs->valueRegister)++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDeci16(asSVMRegisters* regs, const asDWORD* bc) {
    (**(short**)&regs->valueRegister)--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDeci8(asSVMRegisters* regs, const asDWORD* bc) {
    (**(char**)&regs->valueRegister)--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcInci(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(int**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDeci(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(int**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcIncf(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(float**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDecf(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(float**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcIncd(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(double**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDecd(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(double**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcIncVi(asSVMRegisters* regs, const asDWORD* bc) {
    (*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)))++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDecVi(asSVMRegisters* regs, const asDWORD* bc) {
    (*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)))--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcBnot(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = ~*(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcBand(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) & *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBor(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) | *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBxor(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) ^ *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsll(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) << *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsrl(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsra(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(regs->stackFramePointer - asBC_SWORDARG1(bc))) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCopy(asSVMRegisters* regs, const asDWORD* bc) {
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

static int BcPshC8(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= 2;
    *(asQWORD*)regs->stackPointer = asBC_QWORDARG(bc);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcPshVPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcRdsPtr(asSVMRegisters* regs, const asDWORD* bc) {
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

static int BcCmpd(asSVMRegisters* regs, const asDWORD* bc) {
    double dbl1 = *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    double dbl2 = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (dbl1 == dbl2)     *(int*)&regs->valueRegister = 0;
    else if (dbl1 < dbl2) *(int*)&regs->valueRegister = -1;
    else                  *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpu(asSVMRegisters* regs, const asDWORD* bc) {
    asDWORD d1 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asDWORD d2 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpf(asSVMRegisters* regs, const asDWORD* bc) {
    float f1 = *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    float f2 = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (f1 == f2)     *(int*)&regs->valueRegister = 0;
    else if (f1 < f2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpi(asSVMRegisters* regs, const asDWORD* bc) {
    int i1 = *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    int i2 = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpIi(asSVMRegisters* regs, const asDWORD* bc) {
    int i1 = *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    int i2 = asBC_INTARG(bc);
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpIf(asSVMRegisters* regs, const asDWORD* bc) {
    float f1 = *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    float f2 = asBC_FLOATARG(bc);
    if (f1 == f2)     *(int*)&regs->valueRegister = 0;
    else if (f1 < f2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpIu(asSVMRegisters* regs, const asDWORD* bc) {
    asDWORD d1 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asDWORD d2 = asBC_DWORDARG(bc);
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcPopRPtr(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)&regs->valueRegister = *(asPWORD*)regs->stackPointer;
    regs->stackPointer += AS_PTR_SIZE;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPshRPtr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = *(asPWORD*)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcStr(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcSetV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asBC_DWORDARG(bc);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcSetV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asBC_QWORDARG(bc);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcAddSi(asSVMRegisters* regs, const asDWORD* bc) {
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

static int BcCpyVtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCpyVtoV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCpyVtoR4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)&regs->valueRegister = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcCpyVtoR8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)&regs->valueRegister = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcCpyVtoG4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)asBC_PTRARG(bc) = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcCpyRtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asDWORD*)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcCpyRtoV8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcCpyGtoV4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asDWORD*)asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcWrtV1(asSVMRegisters* regs, const asDWORD* bc) {
    **(asBYTE**)&regs->valueRegister = *(asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcWrtV2(asSVMRegisters* regs, const asDWORD* bc) {
    **(asWORD**)&regs->valueRegister = *(asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcWrtV4(asSVMRegisters* regs, const asDWORD* bc) {
    **(asDWORD**)&regs->valueRegister = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcWrtV8(asSVMRegisters* regs, const asDWORD* bc) {
    **(asQWORD**)&regs->valueRegister = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcRdr1(asSVMRegisters* regs, const asDWORD* bc) {
    asBYTE* bPtr = (asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bPtr[0] = **(asBYTE**)&regs->valueRegister;
    bPtr[1] = 0;
    bPtr[2] = 0;
    bPtr[3] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcRdr2(asSVMRegisters* regs, const asDWORD* bc) {
    asWORD* wPtr = (asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    wPtr[0] = **(asWORD**)&regs->valueRegister;
    wPtr[1] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcRdr4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asDWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcRdr8(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = **(asQWORD**)&regs->valueRegister;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcLdg(asSVMRegisters* regs, const asDWORD* bc) {
    *(asPWORD*)&regs->valueRegister = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcLdv(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD**)&regs->valueRegister = (regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPga(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = asBC_PTRARG(bc);
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcCmpPtr(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD p1 = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asPWORD p2 = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (p1 == p2)     *(int*)&regs->valueRegister = 0;
    else if (p1 < p2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcVar(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= AS_PTR_SIZE;
    *(asPWORD*)regs->stackPointer = (asPWORD)asBC_SWORDARG0(bc);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcItof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcFtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcUtof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcFtoU(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asUINT(int(*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcSbtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(signed char*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcSwtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(short*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcUbtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcUwtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDtoU(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asUINT(int(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc))));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDtof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcItod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcUtod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcFtod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcAddi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcSubi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcMuli(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDivi(asSVMRegisters* regs, const asDWORD* bc) {
    int divider = *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    else if (divider == -1) {
        if (*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) == int(0x80000000)) {
            regs->programPointer = const_cast<asDWORD*>(bc);
            Ctx(regs)->SetInternalException(TXT_DIVIDE_OVERFLOW);
            return JITBC_EXIT;
        }
    }
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModi(asSVMRegisters* regs, const asDWORD* bc) {
    int divider = *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    else if (divider == -1) {
        if (*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) == int(0x80000000)) {
            regs->programPointer = const_cast<asDWORD*>(bc);
            Ctx(regs)->SetInternalException(TXT_DIVIDE_OVERFLOW);
            return JITBC_EXIT;
        }
    }
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) % divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcAddf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcSubf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcMulf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDivf(asSVMRegisters* regs, const asDWORD* bc) {
    float divider = *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModf(asSVMRegisters* regs, const asDWORD* bc) {
    float divider = *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = fmodf(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), divider);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcAddd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcSubd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcMuld(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDivd(asSVMRegisters* regs, const asDWORD* bc) {
    double divider = *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModd(asSVMRegisters* regs, const asDWORD* bc) {
    double divider = *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = fmod(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), divider);
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcAddIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcSubIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcMulIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcAddIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcSubIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcMulIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

static int BcSetG4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)asBC_PTRARG(bc) = asBC_DWORDARG(bc + AS_PTR_SIZE);
    regs->programPointer = NextBc(bc, 2 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcItob(asSVMRegisters* regs, const asDWORD* bc) {
    volatile asDWORD val = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    volatile asBYTE* bPtr = (asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bPtr[0] = (asBYTE)val;
    bPtr[1] = 0;
    bPtr[2] = 0;
    bPtr[3] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcItow(asSVMRegisters* regs, const asDWORD* bc) {
    volatile asDWORD val = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    volatile asWORD* wPtr = (asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    wPtr[0] = (asWORD)val;
    wPtr[1] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcI64toi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcUtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcItoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcFtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcFtoU64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asQWORD(asINT64(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc))));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDtoU64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asQWORD(asINT64(*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcI64tof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcU64tof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcI64tod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcU64tod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcNegi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcInci64(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(asQWORD**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDeci64(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(asQWORD**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcBnot64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = ~*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcAddi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcSubi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcMuli64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDivi64(asSVMRegisters* regs, const asDWORD* bc) {
    asINT64 divider = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    else if (divider == -1) {
        if (*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) == (asINT64(1) << 63)) {
            regs->programPointer = const_cast<asDWORD*>(bc);
            Ctx(regs)->SetInternalException(TXT_DIVIDE_OVERFLOW);
            return JITBC_EXIT;
        }
    }
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModi64(asSVMRegisters* regs, const asDWORD* bc) {
    asINT64 divider = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    else if (divider == -1) {
        if (*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) == (asINT64(1) << 63)) {
            regs->programPointer = const_cast<asDWORD*>(bc);
            Ctx(regs)->SetInternalException(TXT_DIVIDE_OVERFLOW);
            return JITBC_EXIT;
        }
    }
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) % divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBand64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) & *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBor64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) | *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBxor64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) ^ *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsll64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) << *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsrl64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcBsra64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpi64(asSVMRegisters* regs, const asDWORD* bc) {
    asINT64 i1 = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asINT64 i2 = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcCmpu64(asSVMRegisters* regs, const asDWORD* bc) {
    asQWORD d1 = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asQWORD d2 = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcClrHi(asSVMRegisters* regs, const asDWORD* bc) {
#if AS_SIZEOF_BOOL == 1
    volatile asBYTE* ptr = (asBYTE*)&regs->valueRegister;
    ptr[1] = 0;
    ptr[2] = 0;
    ptr[3] = 0;
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcPshV8(asSVMRegisters* regs, const asDWORD* bc) {
    regs->stackPointer -= 2;
    *(asQWORD*)regs->stackPointer = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

static int BcDivu(asSVMRegisters* regs, const asDWORD* bc) {
    asUINT divider = *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModu(asSVMRegisters* regs, const asDWORD* bc) {
    asUINT divider = *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) % divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcDivu64(asSVMRegisters* regs, const asDWORD* bc) {
    asQWORD divider = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) / divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcModu64(asSVMRegisters* regs, const asDWORD* bc) {
    asQWORD divider = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    if (divider == 0) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_DIVIDE_BY_ZERO);
        return JITBC_EXIT;
    }
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) % divider;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

static int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc) {
    regs->programPointer = NextBc(bc, 1 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

static int BcCall(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = NextBc(bc, 2);
    ctx->CallScriptFunction(ctx->m_engine->scriptFunctions[i]);
    return JITBC_EXIT;
}

static int BcRet(asSVMRegisters* regs, const asDWORD* bc) {
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

static int BcCallSys(asSVMRegisters* regs, const asDWORD* bc) {
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
    return JITBC_EXIT;
}

static int BcCallBnd(asSVMRegisters* regs, const asDWORD* bc) {
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

static int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    int i = asBC_INTARG(bc);
    regs->programPointer = NextBc(bc, 2);
    ctx->CallInterfaceMethod(ctx->m_engine->GetScriptFunction(i));
    return JITBC_EXIT;
}

static int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCScriptFunction* func = *(asCScriptFunction**)(regs->stackFramePointer - asBC_SWORDARG0(bc));
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
        }
        else {
            regs->programPointer++;
            ctx->CallInterfaceMethod(func->funcForDelegate);
        }
    }
    else if (func->funcType == asFUNC_SYSTEM) {
        regs->stackPointer += CallSystemFunction(func->id, ctx);
        regs->programPointer++;
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
    return JITBC_EXIT;
}

static int BcAlloc(asSVMRegisters* regs, const asDWORD* bc) {
    auto* ctx = Ctx(regs);
    asCObjectType* objType = (asCObjectType*)asBC_PTRARG(bc);
    int func = asBC_INTARG(bc + AS_PTR_SIZE);
    if (objType->flags & asOBJ_SCRIPT_OBJECT) {
        asDWORD* mem = (asDWORD*)ctx->m_engine->CallAlloc(objType);
        ScriptObject_Construct(objType, (asCScriptObject*)mem);
        asCScriptFunction* f = ctx->m_engine->scriptFunctions[func];
        asDWORD** a = (asDWORD**)*(asPWORD*)(regs->stackPointer + f->GetSpaceNeededForArguments());
        if (a) *a = mem;
        regs->stackPointer -= AS_PTR_SIZE;
        *(asPWORD*)regs->stackPointer = (asPWORD)mem;
        regs->programPointer += 2 + AS_PTR_SIZE;
        ctx->CallScriptFunction(f);
        return JITBC_EXIT;
    }
    asDWORD* mem = (asDWORD*)ctx->m_engine->CallAlloc(objType);
    if (func) {
        regs->stackPointer -= AS_PTR_SIZE;
        *(asPWORD*)regs->stackPointer = (asPWORD)mem;
        regs->programPointer = const_cast<asDWORD*>(bc);
        regs->stackPointer += CallSystemFunction(func, ctx);
    }
    asDWORD** a = (asDWORD**)*(asPWORD*)regs->stackPointer;
    regs->stackPointer += AS_PTR_SIZE;
    if (a) *a = mem;
    regs->programPointer += 2 + AS_PTR_SIZE;
    if (regs->doProcessSuspend) {
        if (ctx->m_doSuspend) {
            ctx->m_status = asEXECUTION_SUSPENDED;
            return JITBC_EXIT;
        }
        if (ctx->m_status != asEXECUTION_ACTIVE) {
            ctx->m_engine->CallFree(mem);
            *a = 0;
            return JITBC_EXIT;
        }
    }
    return JITBC_EXIT;
}

static int BcThiscall1(asSVMRegisters* regs, const asDWORD* bc) {
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
    return JITBC_EXIT;
}

}

int JitBcFallback(asSVMRegisters* regs, const asDWORD* bc) {
    switch (static_cast<asEBCInstr>(bc[0] & 0xFF)) {
    case asBC_PopPtr:    return BcPopPtr(regs, bc);
    case asBC_PshGPtr:   return BcPshGPtr(regs, bc);
    case asBC_PshC4:     return BcPshC4(regs, bc);
    case asBC_PshV4:     return BcPshV4(regs, bc);
    case asBC_PSF:       return BcPSF(regs, bc);
    case asBC_SwapPtr:   return BcSwapPtr(regs, bc);
    case asBC_NOT:       return BcNot(regs, bc);
    case asBC_PshG4:     return BcPshG4(regs, bc);
    case asBC_LdGRdR4:   return BcLdGRdR4(regs, bc);
    case asBC_TZ:        return BcTestZero(regs, bc, true);
    case asBC_TNZ:       return BcTestZero(regs, bc, false);
    case asBC_TS:        return BcTestSign(regs, bc, true);
    case asBC_TNS:       return BcTestSign(regs, bc, false);
    case asBC_TP:        return BcTestPos(regs, bc, true);
    case asBC_TNP:       return BcTestPos(regs, bc, false);
    case asBC_NEGi:      return BcNegi(regs, bc);
    case asBC_NEGf:      return BcNegf(regs, bc);
    case asBC_NEGd:      return BcNegd(regs, bc);
    case asBC_INCi16:    return BcInci16(regs, bc);
    case asBC_INCi8:     return BcInci8(regs, bc);
    case asBC_DECi16:    return BcDeci16(regs, bc);
    case asBC_DECi8:     return BcDeci8(regs, bc);
    case asBC_INCi:      return BcInci(regs, bc);
    case asBC_DECi:      return BcDeci(regs, bc);
    case asBC_INCf:      return BcIncf(regs, bc);
    case asBC_DECf:      return BcDecf(regs, bc);
    case asBC_INCd:      return BcIncd(regs, bc);
    case asBC_DECd:      return BcDecd(regs, bc);
    case asBC_IncVi:     return BcIncVi(regs, bc);
    case asBC_DecVi:     return BcDecVi(regs, bc);
    case asBC_BNOT:      return BcBnot(regs, bc);
    case asBC_BAND:      return BcBand(regs, bc);
    case asBC_BOR:       return BcBor(regs, bc);
    case asBC_BXOR:      return BcBxor(regs, bc);
    case asBC_BSLL:      return BcBsll(regs, bc);
    case asBC_BSRL:      return BcBsrl(regs, bc);
    case asBC_BSRA:      return BcBsra(regs, bc);
    case asBC_COPY:      return BcCopy(regs, bc);
    case asBC_PshC8:     return BcPshC8(regs, bc);
    case asBC_PshVPtr:   return BcPshVPtr(regs, bc);
    case asBC_RDSPtr:    return BcRdsPtr(regs, bc);
    case asBC_CMPd:      return BcCmpd(regs, bc);
    case asBC_CMPu:      return BcCmpu(regs, bc);
    case asBC_CMPf:      return BcCmpf(regs, bc);
    case asBC_CMPi:      return BcCmpi(regs, bc);
    case asBC_CMPIi:     return BcCmpIi(regs, bc);
    case asBC_CMPIf:     return BcCmpIf(regs, bc);
    case asBC_CMPIu:     return BcCmpIu(regs, bc);
    case asBC_PopRPtr:   return BcPopRPtr(regs, bc);
    case asBC_PshRPtr:   return BcPshRPtr(regs, bc);
    case asBC_STR:       return BcStr(regs, bc);
    case asBC_SetV4:     return BcSetV4(regs, bc);
    case asBC_SetV8:     return BcSetV8(regs, bc);
    case asBC_ADDSi:     return BcAddSi(regs, bc);
    case asBC_CpyVtoV4:  return BcCpyVtoV4(regs, bc);
    case asBC_CpyVtoV8:  return BcCpyVtoV8(regs, bc);
    case asBC_CpyVtoR4:  return BcCpyVtoR4(regs, bc);
    case asBC_CpyVtoR8:  return BcCpyVtoR8(regs, bc);
    case asBC_CpyVtoG4:  return BcCpyVtoG4(regs, bc);
    case asBC_CpyRtoV4:  return BcCpyRtoV4(regs, bc);
    case asBC_CpyRtoV8:  return BcCpyRtoV8(regs, bc);
    case asBC_CpyGtoV4:  return BcCpyGtoV4(regs, bc);
    case asBC_WRTV1:     return BcWrtV1(regs, bc);
    case asBC_WRTV2:     return BcWrtV2(regs, bc);
    case asBC_WRTV4:     return BcWrtV4(regs, bc);
    case asBC_WRTV8:     return BcWrtV8(regs, bc);
    case asBC_RDR1:      return BcRdr1(regs, bc);
    case asBC_RDR2:      return BcRdr2(regs, bc);
    case asBC_RDR4:      return BcRdr4(regs, bc);
    case asBC_RDR8:      return BcRdr8(regs, bc);
    case asBC_LDG:       return BcLdg(regs, bc);
    case asBC_LDV:       return BcLdv(regs, bc);
    case asBC_PGA:       return BcPga(regs, bc);
    case asBC_CmpPtr:    return BcCmpPtr(regs, bc);
    case asBC_VAR:       return BcVar(regs, bc);
    case asBC_iTOf:      return BcItof(regs, bc);
    case asBC_fTOi:      return BcFtoi(regs, bc);
    case asBC_uTOf:      return BcUtof(regs, bc);
    case asBC_fTOu:      return BcFtoU(regs, bc);
    case asBC_sbTOi:     return BcSbtoi(regs, bc);
    case asBC_swTOi:     return BcSwtoi(regs, bc);
    case asBC_ubTOi:     return BcUbtoi(regs, bc);
    case asBC_uwTOi:     return BcUwtoi(regs, bc);
    case asBC_dTOi:      return BcDtoi(regs, bc);
    case asBC_dTOu:      return BcDtoU(regs, bc);
    case asBC_dTOf:      return BcDtof(regs, bc);
    case asBC_iTOd:      return BcItod(regs, bc);
    case asBC_uTOd:      return BcUtod(regs, bc);
    case asBC_fTOd:      return BcFtod(regs, bc);
    case asBC_ADDi:      return BcAddi(regs, bc);
    case asBC_SUBi:      return BcSubi(regs, bc);
    case asBC_MULi:      return BcMuli(regs, bc);
    case asBC_DIVi:      return BcDivi(regs, bc);
    case asBC_MODi:      return BcModi(regs, bc);
    case asBC_ADDf:      return BcAddf(regs, bc);
    case asBC_SUBf:      return BcSubf(regs, bc);
    case asBC_MULf:      return BcMulf(regs, bc);
    case asBC_DIVf:      return BcDivf(regs, bc);
    case asBC_MODf:      return BcModf(regs, bc);
    case asBC_ADDd:      return BcAddd(regs, bc);
    case asBC_SUBd:      return BcSubd(regs, bc);
    case asBC_MULd:      return BcMuld(regs, bc);
    case asBC_DIVd:      return BcDivd(regs, bc);
    case asBC_MODd:      return BcModd(regs, bc);
    case asBC_ADDIi:     return BcAddIi(regs, bc);
    case asBC_SUBIi:     return BcSubIi(regs, bc);
    case asBC_MULIi:     return BcMulIi(regs, bc);
    case asBC_ADDIf:     return BcAddIf(regs, bc);
    case asBC_SUBIf:     return BcSubIf(regs, bc);
    case asBC_MULIf:     return BcMulIf(regs, bc);
    case asBC_SetG4:     return BcSetG4(regs, bc);
    case asBC_iTOb:      return BcItob(regs, bc);
    case asBC_iTOw:      return BcItow(regs, bc);
    case asBC_i64TOi:    return BcI64toi(regs, bc);
    case asBC_uTOi64:    return BcUtoi64(regs, bc);
    case asBC_iTOi64:    return BcItoi64(regs, bc);
    case asBC_fTOi64:    return BcFtoi64(regs, bc);
    case asBC_dTOi64:    return BcDtoi64(regs, bc);
    case asBC_fTOu64:    return BcFtoU64(regs, bc);
    case asBC_dTOu64:    return BcDtoU64(regs, bc);
    case asBC_i64TOf:    return BcI64tof(regs, bc);
    case asBC_u64TOf:    return BcU64tof(regs, bc);
    case asBC_i64TOd:    return BcI64tod(regs, bc);
    case asBC_u64TOd:    return BcU64tod(regs, bc);
    case asBC_NEGi64:    return BcNegi64(regs, bc);
    case asBC_INCi64:    return BcInci64(regs, bc);
    case asBC_DECi64:    return BcDeci64(regs, bc);
    case asBC_BNOT64:    return BcBnot64(regs, bc);
    case asBC_ADDi64:    return BcAddi64(regs, bc);
    case asBC_SUBi64:    return BcSubi64(regs, bc);
    case asBC_MULi64:    return BcMuli64(regs, bc);
    case asBC_DIVi64:    return BcDivi64(regs, bc);
    case asBC_MODi64:    return BcModi64(regs, bc);
    case asBC_BAND64:    return BcBand64(regs, bc);
    case asBC_BOR64:     return BcBor64(regs, bc);
    case asBC_BXOR64:    return BcBxor64(regs, bc);
    case asBC_BSLL64:    return BcBsll64(regs, bc);
    case asBC_BSRL64:    return BcBsrl64(regs, bc);
    case asBC_BSRA64:    return BcBsra64(regs, bc);
    case asBC_CMPi64:    return BcCmpi64(regs, bc);
    case asBC_CMPu64:    return BcCmpu64(regs, bc);
    case asBC_ClrHi:     return BcClrHi(regs, bc);
    case asBC_PshV8:     return BcPshV8(regs, bc);
    case asBC_DIVu:      return BcDivu(regs, bc);
    case asBC_MODu:      return BcModu(regs, bc);
    case asBC_DIVu64:    return BcDivu64(regs, bc);
    case asBC_MODu64:    return BcModu64(regs, bc);
    case asBC_JitEntry:  return BcJitEntry(regs, bc);
    case asBC_CALL:      return BcCall(regs, bc);
    case asBC_RET:       return BcRet(regs, bc);
    case asBC_CALLSYS:   return BcCallSys(regs, bc);
    case asBC_CALLBND:   return BcCallBnd(regs, bc);
    case asBC_CALLINTF:  return BcCallIntf(regs, bc);
    case asBC_CallPtr:   return BcCallPtr(regs, bc);
    case asBC_ALLOC:     return BcAlloc(regs, bc);
    case asBC_Thiscall1: return BcThiscall1(regs, bc);
    default:
        return JITBC_CONTINUE;
    }
}

}
