#pragma once

#include "angelscript.h"

namespace asjitx86::detail {

int BcPowi(asSVMRegisters* regs, const asDWORD* bc);
int BcPowu(asSVMRegisters* regs, const asDWORD* bc);
int BcPowf(asSVMRegisters* regs, const asDWORD* bc);
int BcPowd(asSVMRegisters* regs, const asDWORD* bc);
int BcPowdi(asSVMRegisters* regs, const asDWORD* bc);
int BcPowi64(asSVMRegisters* regs, const asDWORD* bc);
int BcPowu64(asSVMRegisters* regs, const asDWORD* bc);

}
