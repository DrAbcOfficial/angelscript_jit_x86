#pragma once

#include "bytecode/bc_helpers.h"

#include "as_context.h"

namespace asjitx86::detail {

inline asCContext* Ctx(asSVMRegisters* regs) {
    return static_cast<asCContext*>(regs->ctx);
}

inline asDWORD* NextBc(const asDWORD* bc, int n) {
    return const_cast<asDWORD*>(bc + n);
}

}
