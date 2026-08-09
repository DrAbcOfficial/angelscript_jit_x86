cmake_minimum_required(VERSION 3.24)
project(as_jit_x86 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ASJITX86_BUILD_SHARED "Build as_jit_x86 as a shared library" ON)
option(ASJITX86_BUILD_TESTS  "Build tests" ON)

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

add_subdirectory(third_party/angelscript/sdk/angelscript/projects/cmake)

set(ASMJIT_STATIC TRUE CACHE BOOL "" FORCE)
set(ASMJIT_NO_NATVIS TRUE CACHE BOOL "" FORCE)
add_subdirectory(third_party/asmjit)

if(ASJITX86_BUILD_SHARED)
    add_library(as_jit_x86 SHARED)
    target_compile_definitions(as_jit_x86 PRIVATE ASJITX86_SHARED ASJITX86_EXPORTS)
else()
    add_library(as_jit_x86 STATIC)
    target_compile_definitions(as_jit_x86 PUBLIC ASJITX86_STATIC)
endif()

target_sources(as_jit_x86 PRIVATE
    src/as_jit_api.cpp
    src/jit_engine.cpp
    src/jit_compiler.cpp
)

target_include_directories(as_jit_x86
    PUBLIC  include
    PRIVATE src
    PRIVATE third_party/angelscript/sdk/angelscript/include
    PRIVATE third_party/angelscript/sdk/angelscript/source
)

target_compile_definitions(as_jit_x86 PRIVATE ASMJIT_STATIC)

target_link_libraries(as_jit_x86 PRIVATE angelscript asmjit::asmjit)
