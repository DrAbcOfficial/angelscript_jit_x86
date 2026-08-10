#include "bytecode/helpers/numeric64_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcI64toi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcUtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcItoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcFtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDtoi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asINT64(*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcFtoU64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asQWORD(asINT64(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc))));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDtoU64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asQWORD(asINT64(*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcI64tof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcU64tof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcI64tod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcU64tod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcNegi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcInci64(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(asQWORD**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDeci64(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(asQWORD**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcBnot64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = ~*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcAddi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSubi64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcMuli64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDivi64(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModi64(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcBand64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) & *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBor64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) | *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBxor64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) ^ *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsll64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) << *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsrl64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsra64(asSVMRegisters* regs, const asDWORD* bc) {
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpi64(asSVMRegisters* regs, const asDWORD* bc) {
    asINT64 i1 = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asINT64 i2 = *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (i1 == i2)     *(int*)&regs->valueRegister = 0;
    else if (i1 < i2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcCmpu64(asSVMRegisters* regs, const asDWORD* bc) {
    asQWORD d1 = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    asQWORD d2 = *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc));
    if (d1 == d2)     *(int*)&regs->valueRegister = 0;
    else if (d1 < d2) *(int*)&regs->valueRegister = -1;
    else              *(int*)&regs->valueRegister = 1;
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcClrHi(asSVMRegisters* regs, const asDWORD* bc) {
#if AS_SIZEOF_BOOL == 1
    volatile asBYTE* ptr = (asBYTE*)&regs->valueRegister;
    ptr[1] = 0;
    ptr[2] = 0;
    ptr[3] = 0;
#endif
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDivu(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModu(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcDivu64(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModu64(asSVMRegisters* regs, const asDWORD* bc) {
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

}
