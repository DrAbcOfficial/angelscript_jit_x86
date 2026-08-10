#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcNot(asSVMRegisters* regs, const asDWORD* bc);
int BcNegi(asSVMRegisters* regs, const asDWORD* bc);
int BcNegf(asSVMRegisters* regs, const asDWORD* bc);
int BcNegd(asSVMRegisters* regs, const asDWORD* bc);
int BcInci16(asSVMRegisters* regs, const asDWORD* bc);
int BcInci8(asSVMRegisters* regs, const asDWORD* bc);
int BcDeci16(asSVMRegisters* regs, const asDWORD* bc);
int BcDeci8(asSVMRegisters* regs, const asDWORD* bc);
int BcInci(asSVMRegisters* regs, const asDWORD* bc);
int BcDeci(asSVMRegisters* regs, const asDWORD* bc);
int BcIncf(asSVMRegisters* regs, const asDWORD* bc);
int BcDecf(asSVMRegisters* regs, const asDWORD* bc);
int BcIncd(asSVMRegisters* regs, const asDWORD* bc);
int BcDecd(asSVMRegisters* regs, const asDWORD* bc);
int BcIncVi(asSVMRegisters* regs, const asDWORD* bc);
int BcDecVi(asSVMRegisters* regs, const asDWORD* bc);
int BcBnot(asSVMRegisters* regs, const asDWORD* bc);
int BcBand(asSVMRegisters* regs, const asDWORD* bc);
int BcBor(asSVMRegisters* regs, const asDWORD* bc);
int BcBxor(asSVMRegisters* regs, const asDWORD* bc);
int BcBsll(asSVMRegisters* regs, const asDWORD* bc);
int BcBsrl(asSVMRegisters* regs, const asDWORD* bc);
int BcBsra(asSVMRegisters* regs, const asDWORD* bc);
int BcItof(asSVMRegisters* regs, const asDWORD* bc);
int BcFtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcUtof(asSVMRegisters* regs, const asDWORD* bc);
int BcFtoU(asSVMRegisters* regs, const asDWORD* bc);
int BcSbtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcSwtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcUbtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcUwtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcDtoi(asSVMRegisters* regs, const asDWORD* bc);
int BcDtoU(asSVMRegisters* regs, const asDWORD* bc);
int BcDtof(asSVMRegisters* regs, const asDWORD* bc);
int BcItod(asSVMRegisters* regs, const asDWORD* bc);
int BcUtod(asSVMRegisters* regs, const asDWORD* bc);
int BcFtod(asSVMRegisters* regs, const asDWORD* bc);
int BcAddi(asSVMRegisters* regs, const asDWORD* bc);
int BcSubi(asSVMRegisters* regs, const asDWORD* bc);
int BcMuli(asSVMRegisters* regs, const asDWORD* bc);
int BcDivi(asSVMRegisters* regs, const asDWORD* bc);
int BcModi(asSVMRegisters* regs, const asDWORD* bc);
int BcAddf(asSVMRegisters* regs, const asDWORD* bc);
int BcSubf(asSVMRegisters* regs, const asDWORD* bc);
int BcMulf(asSVMRegisters* regs, const asDWORD* bc);
int BcDivf(asSVMRegisters* regs, const asDWORD* bc);
int BcModf(asSVMRegisters* regs, const asDWORD* bc);
int BcAddd(asSVMRegisters* regs, const asDWORD* bc);
int BcSubd(asSVMRegisters* regs, const asDWORD* bc);
int BcMuld(asSVMRegisters* regs, const asDWORD* bc);
int BcDivd(asSVMRegisters* regs, const asDWORD* bc);
int BcModd(asSVMRegisters* regs, const asDWORD* bc);
int BcAddIi(asSVMRegisters* regs, const asDWORD* bc);
int BcSubIi(asSVMRegisters* regs, const asDWORD* bc);
int BcMulIi(asSVMRegisters* regs, const asDWORD* bc);
int BcAddIf(asSVMRegisters* regs, const asDWORD* bc);
int BcSubIf(asSVMRegisters* regs, const asDWORD* bc);
int BcMulIf(asSVMRegisters* regs, const asDWORD* bc);
int BcSetG4(asSVMRegisters* regs, const asDWORD* bc);
int BcItob(asSVMRegisters* regs, const asDWORD* bc);
int BcItow(asSVMRegisters* regs, const asDWORD* bc);

}
