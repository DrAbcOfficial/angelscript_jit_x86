#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcPopPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcPshGPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcPshC4(asSVMRegisters* regs, const asDWORD* bc);
int BcPshV4(asSVMRegisters* regs, const asDWORD* bc);
int BcPSF(asSVMRegisters* regs, const asDWORD* bc);
int BcSwapPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcPshG4(asSVMRegisters* regs, const asDWORD* bc);
int BcLdGRdR4(asSVMRegisters* regs, const asDWORD* bc);
int BcCopy(asSVMRegisters* regs, const asDWORD* bc);
int BcPshC8(asSVMRegisters* regs, const asDWORD* bc);
int BcPshVPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcRdsPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcPopRPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcPshRPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcStr(asSVMRegisters* regs, const asDWORD* bc);
int BcSetV4(asSVMRegisters* regs, const asDWORD* bc);
int BcSetV8(asSVMRegisters* regs, const asDWORD* bc);
int BcAddSi(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyVtoV4(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyVtoV8(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyVtoR4(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyVtoR8(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyVtoG4(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyRtoV4(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyRtoV8(asSVMRegisters* regs, const asDWORD* bc);
int BcCpyGtoV4(asSVMRegisters* regs, const asDWORD* bc);
int BcWrtV1(asSVMRegisters* regs, const asDWORD* bc);
int BcWrtV2(asSVMRegisters* regs, const asDWORD* bc);
int BcWrtV4(asSVMRegisters* regs, const asDWORD* bc);
int BcWrtV8(asSVMRegisters* regs, const asDWORD* bc);
int BcRdr1(asSVMRegisters* regs, const asDWORD* bc);
int BcRdr2(asSVMRegisters* regs, const asDWORD* bc);
int BcRdr4(asSVMRegisters* regs, const asDWORD* bc);
int BcRdr8(asSVMRegisters* regs, const asDWORD* bc);
int BcLdg(asSVMRegisters* regs, const asDWORD* bc);
int BcLdv(asSVMRegisters* regs, const asDWORD* bc);
int BcPga(asSVMRegisters* regs, const asDWORD* bc);
int BcVar(asSVMRegisters* regs, const asDWORD* bc);
int BcPshV8(asSVMRegisters* regs, const asDWORD* bc);

}
