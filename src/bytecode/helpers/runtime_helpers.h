#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc);
int BcCall(asSVMRegisters* regs, const asDWORD* bc);
int BcRet(asSVMRegisters* regs, const asDWORD* bc);
int BcCallSys(asSVMRegisters* regs, const asDWORD* bc);
int BcCallBnd(asSVMRegisters* regs, const asDWORD* bc);
int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc);
int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcThiscall1(asSVMRegisters* regs, const asDWORD* bc);
int BcJmpP(asSVMRegisters* regs, const asDWORD* bc);
int BcSuspend(asSVMRegisters* regs, const asDWORD* bc);

}
