#pragma once

#include "angelscript.h"

#include <asmjit/x86.h>

namespace asjitx86 {

int EmitFunction(asmjit::JitRuntime& runtime, asIScriptFunction* function, asJITFunction* out);

}
