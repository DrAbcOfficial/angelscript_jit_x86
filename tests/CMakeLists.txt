add_executable(asjitx86_tests
    consistency/jit_consistency.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptarray/scriptarray.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptbuilder/scriptbuilder.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
)

target_include_directories(asjitx86_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/angelscript/include
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptarray
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptbuilder
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptstdstring
)

target_link_libraries(asjitx86_tests PRIVATE as_jit_x86 angelscript)

if(UNIX)
    target_link_libraries(asjitx86_tests PRIVATE pthread dl)
endif()

if(WIN32)
    add_custom_command(TARGET asjitx86_tests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:as_jit_x86>
                $<TARGET_FILE_DIR:asjitx86_tests>)
endif()

add_executable(asjitx86_benchmark performance/jit_benchmark.cpp)
target_include_directories(asjitx86_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/angelscript/include
)
target_link_libraries(asjitx86_benchmark PRIVATE as_jit_x86 angelscript)

if(WIN32)
    add_custom_command(TARGET asjitx86_benchmark POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:as_jit_x86>
                $<TARGET_FILE_DIR:asjitx86_benchmark>)
endif()

add_executable(asjitx86_showcase
    performance/jit_showcase.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptarray/scriptarray.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptdictionary/scriptdictionary.cpp
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptstdstring/scriptstdstring.cpp
)

target_include_directories(asjitx86_showcase PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/angelscript/include
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptarray
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptdictionary
    ${CMAKE_SOURCE_DIR}/third_party/angelscript/sdk/add_on/scriptstdstring
)

target_link_libraries(asjitx86_showcase PRIVATE as_jit_x86 angelscript)

if(UNIX)
    target_link_libraries(asjitx86_showcase PRIVATE pthread dl)
endif()

if(WIN32)
    add_custom_command(TARGET asjitx86_showcase POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:as_jit_x86>
                $<TARGET_FILE_DIR:asjitx86_showcase>)
endif()

add_test(NAME jit_consistency COMMAND asjitx86_tests
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
