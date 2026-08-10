#include "bytecode/helpers/power_helpers.h"
#include "bytecode/helpers/helper_context.h"

#include "as_scriptengine.h"
#include "as_scriptobject.h"
#include "as_texts.h"

#include <cassert>
#include <cmath>
#include <cstring>

namespace asjitx86::detail {

int BcPowi(asSVMRegisters* regs, const asDWORD* bc) {
    bool isOverflow;
    *(int*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = as_powi(*(int*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc)), isOverflow);
    if (isOverflow) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowu(asSVMRegisters* regs, const asDWORD* bc) {
    bool isOverflow;
    *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = as_powu(*(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(asDWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc)), isOverflow);
    if (isOverflow) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowf(asSVMRegisters* regs, const asDWORD* bc) {
    float r = powf(*(float*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(float*)(regs->stackFramePointer - asBC_SWORDARG2(bc)));
    *(float*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = r;
    if (r == float(HUGE_VAL)) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowd(asSVMRegisters* regs, const asDWORD* bc) {
    double r = pow(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(double*)(regs->stackFramePointer - asBC_SWORDARG2(bc)));
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = r;
    if (r == HUGE_VAL) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowdi(asSVMRegisters* regs, const asDWORD* bc) {
    double r = pow(*(double*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(int*)(regs->stackFramePointer - asBC_SWORDARG2(bc)));
    *(double*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = r;
    if (r == HUGE_VAL) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowi64(asSVMRegisters* regs, const asDWORD* bc) {
    bool isOverflow;
    *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = as_powi64(*(asINT64*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(asINT64*)(regs->stackFramePointer - asBC_SWORDARG2(bc)), isOverflow);
    if (isOverflow) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

int BcPowu64(asSVMRegisters* regs, const asDWORD* bc) {
    bool isOverflow;
    *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG0(bc)) = as_powu64(*(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG1(bc)), *(asQWORD*)(regs->stackFramePointer - asBC_SWORDARG2(bc)), isOverflow);
    if (isOverflow) {
        regs->programPointer = const_cast<asDWORD*>(bc);
        Ctx(regs)->SetInternalException(TXT_POW_OVERFLOW);
        return JITBC_EXIT;
    }
    regs->programPointer = NextBc(bc, 2);
    return JITBC_CONTINUE;
}

}
