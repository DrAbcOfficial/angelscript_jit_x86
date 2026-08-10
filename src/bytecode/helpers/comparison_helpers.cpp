#include "bytecode/helpers/comparison_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcTestZero(asSVMRegisters* regs, const asDWORD* bc, bool zero) {
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

int BcTestSign(asSVMRegisters* regs, const asDWORD* bc, bool neg) {
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

int BcTestPos(asSVMRegisters* regs, const asDWORD* bc, bool pos) {
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

int BcCmpd(asSVMRegisters* regs, const asDWORD* bc) {
    double dbl1 = *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    double dbl2 = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (dbl1 == dbl2)     *(int*)&regs->valueRegister = 0;
    else if (dbl1 < dbl2) *(int*)&regs->valueRegister = -1;
    else                  *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpu(asSVMRegisters* regs, const asDWORD* bc) {
    asDWORD d1 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asDWORD d2 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpf(asSVMRegisters* regs, const asDWORD* bc) {
    float f1 = *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    float f2 = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (f1 == f2)     *(int*)&regs->valueRegister = 0;
    else if (f1 < f2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpi(asSVMRegisters* regs, const asDWORD* bc) {
    int i1 = *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    int i2 = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpIi(asSVMRegisters* regs, const asDWORD* bc) {
    int i1 = *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    int i2 = asBC_INTARG(bc);
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpIf(asSVMRegisters* regs, const asDWORD* bc) {
    float f1 = *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    float f2 = asBC_FLOATARG(bc);
    if (f1 == f2)     *(int*)&regs->valueRegister = 0;
    else if (f1 < f2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpIu(asSVMRegisters* regs, const asDWORD* bc) {
    asDWORD d1 = *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asDWORD d2 = asBC_DWORDARG(bc);
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpPtr(asSVMRegisters* regs, const asDWORD* bc) {
    asPWORD p1 = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asPWORD p2 = *(asPWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (p1 == p2)     *(int*)&regs->valueRegister = 0;
    else if (p1 < p2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

}
