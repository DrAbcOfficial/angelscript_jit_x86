#include "codegen/emit.h"
#include "codegen/emit/emitter.h"

namespace asjitx86 {

int EmitFunction(asmjit::JitRuntime& runtime,
                 detail::ScalarObjectPool& objectPool,
                 asIScriptFunction* function, asJITFunction* out) {
    return emit::FunctionEmitter(runtime, objectPool, function, out).Run();
}

}
