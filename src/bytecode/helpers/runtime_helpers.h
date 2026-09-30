#pragma once

#include "angelscript.h"

class asCContext;
class asCScriptFunction;
struct asSTryCatchInfo;

namespace asjitx86::detail {

inline constexpr unsigned kMaxDirectJitCallDepth = 32;
inline constexpr int kJitBcCaught = 2;

int ResumeJitCallChain(asSVMRegisters* regs, asUINT callerCallStackLength,
                       unsigned maxDirectDepth = kMaxDirectJitCallDepth);
int CallScriptFunction(asSVMRegisters* regs, asCScriptFunction* function,
                       const asDWORD* nextBc);
int FastCallSimpleScript(asSVMRegisters* regs, asCScriptFunction* function,
                         const asDWORD* nextBc);
bool CanFastCallSimpleScript(asCScriptFunction* function);
int CallFunctionPointer(asSVMRegisters* regs, asCScriptFunction* function,
                        const asDWORD* callBc, const asDWORD* nextBc);
void ReleaseScriptFunction(asCScriptFunction* function);
int BcJitEntry(asSVMRegisters* regs, const asDWORD* bc);
int BcCall(asSVMRegisters* regs, const asDWORD* bc);
int BcRet(asSVMRegisters* regs, const asDWORD* bc);
int BcCallSys(asSVMRegisters* regs, const asDWORD* bc);
bool CanUseFastSystemCall(asCScriptFunction* function);
int FastSystemCall(asSVMRegisters* regs, asCScriptFunction* function);
// Minimal cdecl shims for direct JIT system calls: they keep the C++
// exception translation (a raw call from generated code would let an
// application exception unwind through JIT frames) without FastSystemCall's
// dispatch and bookkeeping, which the JIT emits inline instead.
asDWORD TryCallCdecl0(asCContext* ctx, asFUNCTION_t function);
asDWORD TryCallCdecl1(asCContext* ctx, asFUNCTION_t function, asDWORD a);
asDWORD TryCallCdecl2(asCContext* ctx, asFUNCTION_t function, asDWORD a,
                      asDWORD b);
asDWORD TryCallCdecl3(asCContext* ctx, asFUNCTION_t function, asDWORD a,
                      asDWORD b, asDWORD c);
asDWORD TryCallCdecl4(asCContext* ctx, asFUNCTION_t function, asDWORD a,
                      asDWORD b, asDWORD c, asDWORD d);
int FinishSystemCall(asSVMRegisters* regs);
int FinishSystemCallAt(asSVMRegisters* regs, asCScriptFunction* function,
                       const asSTryCatchInfo* catchInfo,
                       int catchNeedsCleanup);
void RaiseInternalException(asSVMRegisters* regs, const asDWORD* bc,
                            const char* message);
int RaiseAndCatchInternalException(asSVMRegisters* regs, const asDWORD* bc,
                                   const char* message,
                                   asCScriptFunction* function,
                                   const asSTryCatchInfo* catchInfo,
                                   int catchNeedsCleanup);
int BcCallBnd(asSVMRegisters* regs, const asDWORD* bc);
int BcCallIntf(asSVMRegisters* regs, const asDWORD* bc);
int BcCallPtr(asSVMRegisters* regs, const asDWORD* bc);
int BcThiscall1(asSVMRegisters* regs, const asDWORD* bc);
int BcJmpP(asSVMRegisters* regs, const asDWORD* bc);
int BcSuspend(asSVMRegisters* regs, const asDWORD* bc);

}
