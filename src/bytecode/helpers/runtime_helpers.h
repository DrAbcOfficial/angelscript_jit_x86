#pragma once

#include "angelscript.h"

class asCScriptFunction;

namespace asjitx86::detail {

inline constexpr unsigned kMaxDirectJitCallDepth = 32;
inline constexpr int kJitBcCaught = 2;

int ResumeJitCallChain(asSVMRegisters* regs, asUINT callerCallStackLength,
                       unsigned maxDirectDepth = kMaxDirectJitCallDepth);
int CallScriptFunction(asSVMRegisters* regs, asCScriptFunction* function,
                       const asDWORD* nextBc);
int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc);
int BcCall(asSVMRegisters* regs, const asDWORD* bc);
int BcRet(asSVMRegisters* regs, const asDWORD* bc);
int BcCallSys(asSVMRegisters* regs, const asDWORD* bc);
bool CanUseFastSystemCall(asCScriptFunction* function);
int FastSystemCall(asSVMRegisters* regs, asCScriptFunction* function);
int FinishSystemCall(asSVMRegisters* regs);
int FinishSystemCallAt(asSVMRegisters* regs, asCScriptFunction* function,
                       const asDWORD* catchBc);
void RaiseInternalException(asSVMRegisters* regs, const asDWORD* bc,
                            const char* message);
int RaiseAndCatchInternalException(asSVMRegisters* regs, const asDWORD* bc,
                                   const char* message,
                                   asCScriptFunction* function,
                                   const asDWORD* catchBc);
int BcCallBnd(asSVMRegisters* regs, const asDWORD* bc);
int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc);
int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcThiscall1(asSVMRegisters* regs, const asDWORD* bc);
int BcJmpP(asSVMRegisters* regs, const asDWORD* bc);
int BcSuspend(asSVMRegisters* regs, const asDWORD* bc);

}
