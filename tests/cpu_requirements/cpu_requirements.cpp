#include "api/cpu_requirements.h"

#include <cstdio>
#include <cstring>

namespace {

bool Contains(const char* value, const char* expected) {
    return value && std::strstr(value, expected);
}

}

int main() {
    asmjit::CpuFeatures features;
    const char* error = asjitx86::detail::CheckCpuCompatibility(features);
    if (!Contains(error, "SSE2")) {
        std::printf("missing SSE2 was not rejected\n");
        return 1;
    }

    features.add(
        asmjit::CpuFeatures::X86::kSSE,
        asmjit::CpuFeatures::X86::kSSE2);
    error = asjitx86::detail::CheckCpuCompatibility(features);

#if ASJITX86_ENABLE_AVX2
    if (!Contains(error, "AVX2")) {
        std::printf("missing AVX2 was not rejected\n");
        return 1;
    }

    features.add(
        asmjit::CpuFeatures::X86::kAVX,
        asmjit::CpuFeatures::X86::kAVX2);
    error = asjitx86::detail::CheckCpuCompatibility(features);
#endif

    if (error) {
        std::printf("supported feature set was rejected: %s\n", error);
        return 1;
    }

    std::printf("CPU requirement checks passed\n");
    return 0;
}
