#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcTestZero(asSVMRegisters* regs, const asDWORD* bc, bool zero);
int BcTestSign(asSVMRegisters* regs, const asDWORD* bc, bool neg);
int BcTestPos(asSVMRegisters* regs, const asDWORD* bc, bool pos);
int BcCmpd(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpu(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpf(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpi(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpIi(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpIf(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpIu(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpPtr(asSVMRegisters* regs, const asDWORD* bc);

}
