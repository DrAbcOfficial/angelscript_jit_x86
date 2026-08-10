#pragma once

#include "angelscript.h"

namespace asjitx86 {

enum JitBcResult {
    JITBC_CONTINUE = 0,
    JITBC_EXIT     = 1,
};

int JitBcFallback(asSVMRegisters* regs, const asDWORD* bc);

}
