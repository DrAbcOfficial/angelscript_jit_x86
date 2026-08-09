#ifndef AS_JIT_X86_H
#define AS_JIT_X86_H

#if defined(_WIN32) && defined(ASJITX86_SHARED)
#  ifdef ASJITX86_EXPORTS
#    define ASJITX86_API __declspec(dllexport)
#  else
#    define ASJITX86_API __declspec(dllimport)
#  endif
#else
#  define ASJITX86_API
#endif

class asIScriptEngine;

#ifdef __cplusplus
extern "C" {
#endif

ASJITX86_API void* AsJitCreateEngine(asIScriptEngine* engine);
ASJITX86_API void  AsJitDestroyEngine(void* jitEngine);

#ifdef __cplusplus
}
#endif

#endif
