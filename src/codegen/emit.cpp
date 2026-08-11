#include "codegen/emit.h"
#include "codegen/emit/emitter.h"

namespace asjitx86 {

int EmitFunction(asmjit::JitRuntime& runtime, asIScriptFunction* function,
                 asJITFunction* out) {
    return emit::FunctionEmitter(runtime, function, out).Run();
}

}
