#pragma once

#include "angelscript.h"

namespace asjitx86 {

enum JitBcResult {
    JITBC_CONTINUE = 0,
    JITBC_EXIT     = 1,
};

using JitBcHelper = int (*)(asSVMRegisters*, const asDWORD*);

JitBcHelper GetJitBcHelper(asEBCInstr op);

}
