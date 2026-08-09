#include "bc_helpers.h"

namespace asjitx86 {

int JitBcFallback(asSVMRegisters* regs, const asDWORD* bc) {
    (void)regs;
    (void)bc;
    return JITBC_CONTINUE;
}

}
