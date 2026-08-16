#ifndef AS_JIT_X86_H
#define AS_JIT_X86_H

#if defined(_WIN32) && defined(ASJITX86_SHARED)
#  ifdef ASJITX86_EXPORTS
#    define ASJITX86_API __declspec(dllexport)
#  else
#    define ASJITX86_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(ASJITX86_SHARED)
#  define ASJITX86_API __attribute__((visibility("default")))
#else
#  define ASJITX86_API
#endif

class asIScriptEngine;

#ifdef __cplusplus
extern "C" {
#endif

ASJITX86_API void* AsJitCreateEngine(asIScriptEngine* engine);
ASJITX86_API void  AsJitDestroyEngine(void* jitEngine);
ASJITX86_API const char* AsJitGetCompatibilityError(void);

#ifdef __cplusplus
}
#endif

#endif
