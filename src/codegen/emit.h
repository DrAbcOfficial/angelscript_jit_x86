#pragma once

#include "angelscript.h"

#include <asmjit/x86.h>

namespace asjitx86 {

namespace detail {
class ScalarObjectPool;
}

int EmitFunction(asmjit::JitRuntime& runtime,
                 detail::ScalarObjectPool& objectPool,
                 asIScriptFunction* function, asJITFunction* out);

}
