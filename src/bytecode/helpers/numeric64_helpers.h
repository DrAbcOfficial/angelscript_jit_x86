#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcI64toi(asSVMRegisters* regs, const asDWORD* bc);
int BcUtoi64(asSVMRegisters* regs, const asDWORD* bc);
int BcItoi64(asSVMRegisters* regs, const asDWORD* bc);
int BcFtoi64(asSVMRegisters* regs, const asDWORD* bc);
int BcDtoi64(asSVMRegisters* regs, const asDWORD* bc);
int BcFtoU64(asSVMRegisters* regs, const asDWORD* bc);
int BcDtoU64(asSVMRegisters* regs, const asDWORD* bc);
int BcI64tof(asSVMRegisters* regs, const asDWORD* bc);
int BcU64tof(asSVMRegisters* regs, const asDWORD* bc);
int BcI64tod(asSVMRegisters* regs, const asDWORD* bc);
int BcU64tod(asSVMRegisters* regs, const asDWORD* bc);
int BcNegi64(asSVMRegisters* regs, const asDWORD* bc);
int BcInci64(asSVMRegisters* regs, const asDWORD* bc);
int BcDeci64(asSVMRegisters* regs, const asDWORD* bc);
int BcBnot64(asSVMRegisters* regs, const asDWORD* bc);
int BcAddi64(asSVMRegisters* regs, const asDWORD* bc);
int BcSubi64(asSVMRegisters* regs, const asDWORD* bc);
int BcMuli64(asSVMRegisters* regs, const asDWORD* bc);
int BcDivi64(asSVMRegisters* regs, const asDWORD* bc);
int BcModi64(asSVMRegisters* regs, const asDWORD* bc);
int BcBand64(asSVMRegisters* regs, const asDWORD* bc);
int BcBor64(asSVMRegisters* regs, const asDWORD* bc);
int BcBxor64(asSVMRegisters* regs, const asDWORD* bc);
int BcBsll64(asSVMRegisters* regs, const asDWORD* bc);
int BcBsrl64(asSVMRegisters* regs, const asDWORD* bc);
int BcBsra64(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpi64(asSVMRegisters* regs, const asDWORD* bc);
int BcCmpu64(asSVMRegisters* regs, const asDWORD* bc);
int BcClrHi(asSVMRegisters* regs, const asDWORD* bc);
int BcDivu(asSVMRegisters* regs, const asDWORD* bc);
int BcModu(asSVMRegisters* regs, const asDWORD* bc);
int BcDivu64(asSVMRegisters* regs, const asDWORD* bc);
int BcModu64(asSVMRegisters* regs, const asDWORD* bc);

}
