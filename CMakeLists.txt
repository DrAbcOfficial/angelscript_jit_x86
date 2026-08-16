cmake_minimum_required(VERSION 3.24)
project(as_jit_x86 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASJITX86_BUILD_SHARED "Build as_jit_x86 as a shared library" ON)
option(ASJITX86_BUILD_TESTS  "Build tests" ON)
option(ASJITX86_ENABLE_SSE   "Enable SSE2-optimized JIT code generation" ON)
option(ASJITX86_ENABLE_AVX   "Enable AVX-optimized JIT code generation" OFF)

if(ASJITX86_ENABLE_AVX AND NOT ASJITX86_ENABLE_SSE)
    message(FATAL_ERROR "ASJITX86_ENABLE_AVX requires ASJITX86_ENABLE_SSE")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "as_jit_x86 targets x86 (32-bit) only. "
        "Windows: configure with -A Win32. Linux: use -DCMAKE_CXX_FLAGS=-m32 -DCMAKE_C_FLAGS=-m32")
endif()

if(UNIX)
    add_compile_options(-m32)
    add_link_options(-m32)
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(third_party/angelscript/sdk/angelscript/projects/cmake EXCLUDE_FROM_ALL)
get_target_property(ANGELSCRIPT_COMPILE_DEFINITIONS angelscript COMPILE_DEFINITIONS)
list(REMOVE_ITEM ANGELSCRIPT_COMPILE_DEFINITIONS ANGELSCRIPT_EXPORT)
set_target_properties(angelscript PROPERTIES
    COMPILE_DEFINITIONS "${ANGELSCRIPT_COMPILE_DEFINITIONS}"
)

set(ASMJIT_STATIC TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_NATVIS TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_INSTALL TRUE CACHE BOOL "" FORCE)
add_subdirectory(third_party/asmjit EXCLUDE_FROM_ALL)

if(ASJITX86_BUILD_SHARED)
    add_library(as_jit_x86 SHARED)
    target_compile_definitions(as_jit_x86 PRIVATE ASJITX86_SHARED ASJITX86_EXPORTS)
    set_target_properties(as_jit_x86 PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    if(UNIX AND NOT APPLE)
        target_link_options(as_jit_x86 PRIVATE "LINKER:--exclude-libs,ALL")
    endif()
else()
    add_library(as_jit_x86 STATIC)
    target_compile_definitions(as_jit_x86 PUBLIC ASJITX86_STATIC)
endif()

target_sources(as_jit_x86 PRIVATE
    src/api/as_jit_api.cpp
    src/engine/jit_engine.cpp
    src/compiler/jit_compiler.cpp
    src/codegen/emit.cpp
    src/codegen/emit/analysis.cpp
    src/codegen/emit/calls.cpp
    src/codegen/emit/control_flow.cpp
    src/codegen/emit/emitter.cpp
    src/codegen/emit/memory.cpp
    src/codegen/emit/numeric.cpp
    src/codegen/emit/references.cpp
    src/codegen/emit/stack.cpp
    src/bytecode/bc_helpers.cpp
    src/bytecode/helpers/stack_memory_helpers.cpp
    src/bytecode/helpers/comparison_helpers.cpp
    src/bytecode/helpers/numeric32_helpers.cpp
    src/bytecode/helpers/numeric64_helpers.cpp
    src/bytecode/helpers/runtime_helpers.cpp
    src/bytecode/helpers/object_helpers.cpp
    src/bytecode/helpers/power_helpers.cpp
    src/bytecode/bc_info.cpp
)

target_include_directories(as_jit_x86
    PUBLIC  include
    PRIVATE src
    PRIVATE third_party/angelscript/sdk/angelscript/include
    PRIVATE third_party/angelscript/sdk/angelscript/source
)

target_compile_definitions(as_jit_x86 PRIVATE
    ASMJIT_STATIC
    ASJITX86_ENABLE_SSE=$<BOOL:${ASJITX86_ENABLE_SSE}>
    ASJITX86_ENABLE_AVX=$<BOOL:${ASJITX86_ENABLE_AVX}>
)

target_link_libraries(as_jit_x86 PRIVATE angelscript asmjit::asmjit)

if(ASJITX86_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(GNUInstallDirs)

install(TARGETS as_jit_x86
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES include/as_jit_x86.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
    LICENSE
    THIRD_PARTY_LICENSES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/as_jit_x86
)
