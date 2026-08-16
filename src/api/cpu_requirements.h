#pragma once

#include <asmjit/core.h>

namespace asjitx86::detail {

inline const char* CheckCpuCompatibility(
    const asmjit::CpuFeatures& features) noexcept {
    if (!features.x86().has_sse2()) {
        return "SSE2 CPU support is required by this JIT build";
    }

#if ASJITX86_ENABLE_AVX2
    if (!features.x86().has_avx2()) {
        return "AVX2 CPU and operating system support is required by this JIT build";
    }
#endif

    return nullptr;
}

}
