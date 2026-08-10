#include "bytecode/helpers/numeric32_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcNot(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcNegi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asDWORD(-int(*(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcNegf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcNegd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = -*(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcInci16(asSVMRegisters* regs, const asDWORD* bc) {
    (**(short**)&regs->valueRegister)++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcInci8(asSVMRegisters* regs, const asDWORD* bc) {
    (**(char**)&regs->valueRegister)++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDeci16(asSVMRegisters* regs, const asDWORD* bc) {
    (**(short**)&regs->valueRegister)--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDeci8(asSVMRegisters* regs, const asDWORD* bc) {
    (**(char**)&regs->valueRegister)--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcInci(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(int**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDeci(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(int**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcIncf(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(float**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDecf(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(float**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcIncd(asSVMRegisters* regs, const asDWORD* bc) {
    ++(**(double**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDecd(asSVMRegisters* regs, const asDWORD* bc) {
    --(**(double**)&regs->valueRegister);
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcIncVi(asSVMRegisters* regs, const asDWORD* bc) {
    (*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)))++;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDecVi(asSVMRegisters* regs, const asDWORD* bc) {
    (*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)))--;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcBnot(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = ~*(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcBand(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) & *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBor(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) | *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBxor(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) ^ *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsll(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) << *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsrl(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(regs->stackFramePointer - asBC_SWORDARG1(bc)) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcBsra(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(regs->stackFramePointer - asBC_SWORDARG1(bc))) >> *(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcItof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcFtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcUtof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(regs->stackFramePointer - asBC_SWORDARG0(bc)));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcFtoU(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asUINT(int(*(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc))));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcSbtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(signed char*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcSwtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(short*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcUbtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcUwtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcDtoi(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = int(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDtoU(asSVMRegisters* regs, const asDWORD* bc) {
    *(regs->stackFramePointer - asBC_SWORDARG0(bc)) = asUINT(int(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc))));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDtof(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = float(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcItod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcUtod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(asUINT*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcFtod(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = double(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcAddi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSubi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcMuli(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDivi(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModi(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcAddf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSubf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcMulf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDivf(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModf(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcAddd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcSubd(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcMuld(asSVMRegisters* regs, const asDWORD* bc) {
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc));
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcDivd(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcModd(asSVMRegisters* regs, const asDWORD* bc) {
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

int BcAddIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcSubIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcMulIi(asSVMRegisters* regs, const asDWORD* bc) {
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * asBC_INTARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcAddIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) + asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcSubIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) - asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcMulIf(asSVMRegisters* regs, const asDWORD* bc) {
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = *(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)) * asBC_FLOATARG(bc + 1);
    regs->programPointer = NextBc(bc, 3);
    return JITBC_CONTINUE;
}

int BcSetG4(asSVMRegisters* regs, const asDWORD* bc) {
    *(asDWORD*)asBC_PTRARG(bc) = asBC_DWORDARG(bc + AS_PTR_SIZE);
    regs->programPointer = NextBc(bc, 2 + AS_PTR_SIZE);
    return JITBC_CONTINUE;
}

int BcItob(asSVMRegisters* regs, const asDWORD* bc) {
    volatile asDWORD val = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    volatile asBYTE* bPtr = (asBYTE*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    bPtr[0] = (asBYTE)val;
    bPtr[1] = 0;
    bPtr[2] = 0;
    bPtr[3] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

int BcItow(asSVMRegisters* regs, const asDWORD* bc) {
    volatile asDWORD val = *(regs->stackFramePointer - asBC_SWORDARG0(bc));
    volatile asWORD* wPtr = (asWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc));
    wPtr[0] = (asWORD)val;
    wPtr[1] = 0;
    regs->programPointer = NextBc(bc, 1);
    return JITBC_CONTINUE;
}

}
