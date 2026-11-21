cmake_minimum_required(VERSION 3.25)
# macOS: what players run (the runtime, qilin_run) is built for macOS 10.15,
# the oldest macOS a Vulkan 1.3 MoltenVK runs on; so it avoids std::format,
# which Apple's libc++ only has from 13.3 (recomp/runtime/strf.hpp). The
# developer tools (qilin, the tests) need 13.3 and are built for it.
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET 10.15 CACHE STRING "oldest macOS the runtime runs on")
endif()
# The single source of the version number (see CHANGELOG.md for the scheme).
project(BarbieWildHorseRescue-QilinRecomp VERSION 0.1.0 LANGUAGES C CXX)

# ---- Language: C++23 everywhere (recompiler, runtime, generated code) ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Minimum compilers for the C++23 language and library features we use:
# GCC 14, Clang 19, Xcode 16 (AppleClang 16), Visual Studio 2022 17.10
# (std::expected, std::print, std::ranges::to, std::string::contains).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "GCC ${CMAKE_CXX_COMPILER_VERSION} is too old: C++23 <print> needs GCC 14 or newer")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
    # Clang 18 lacks the C++20 concepts level libstdc++ requires for std::expected.
    message(FATAL_ERROR "Clang ${CMAKE_CXX_COMPILER_VERSION} is too old: needs Clang 19 or newer")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 16)
    message(FATAL_ERROR "AppleClang ${CMAKE_CXX_COMPILER_VERSION} is too old: needs Xcode 16 or newer")
elseif(MSVC AND MSVC_VERSION LESS 1940)
    message(FATAL_ERROR "MSVC ${MSVC_VERSION} is too old: needs Visual Studio 2022 17.10 or newer")
endif()

option(QILIN_WERROR "Treat warnings in our own code as errors" OFF)

# ---- Zydis (vendored, MIT) ----
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_MAN OFF CACHE BOOL "" FORCE)
set(ZYDIS_BUILD_SHARED_LIB OFF CACHE BOOL "" FORCE)
set(ZYDIS_FEATURE_ENCODER OFF CACHE BOOL "" FORCE)
set(ZYDIS_FEATURE_AVX512 OFF CACHE BOOL "" FORCE)
set(ZYDIS_FEATURE_KNC OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/zydis EXCLUDE_FROM_ALL)

# ---- Warnings for our own targets ----
add_library(qilin_warnings INTERFACE)
if(MSVC)
    target_compile_options(qilin_warnings INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor)
    # fopen/strerror etc. are standard C++; MSVC's "use the _s variants" advice is not.
    target_compile_definitions(qilin_warnings INTERFACE _CRT_SECURE_NO_WARNINGS)
    if(QILIN_WERROR)
        target_compile_options(qilin_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(qilin_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
    if(QILIN_WERROR)
        target_compile_options(qilin_warnings INTERFACE -Werror)
    endif()
endif()

# ---- Sanitizers for the hand-written runtime (developer option) ----
# -DQILIN_SANITIZE=address,undefined (or thread) builds the runtime's own
# libraries and tests with those sanitizers and links every program with
# them. The generated code in qilin_run is left as it is: it's far too
# large to instrument, and its memory is the guest's, which it checks itself.
set(QILIN_SANITIZE "" CACHE STRING "Sanitizers for the runtime's own code (GCC/Clang), e.g. address,undefined")
add_library(qilin_sanitize INTERFACE)
if(QILIN_SANITIZE AND NOT MSVC)
    # (GCC's instrumented builds raise false array-bounds and overflow warnings.)
    target_compile_options(qilin_sanitize INTERFACE -fsanitize=${QILIN_SANITIZE} -fno-omit-frame-pointer -g -Wno-error)
    add_link_options(-fsanitize=${QILIN_SANITIZE})
endif()
target_link_libraries(qilin_warnings INTERFACE qilin_sanitize)

# ---- Guest runtime (header-only) and the floating-point rules lifted code needs ----
# Lifted SSE code must be bit-exact IEEE single precision: no multiply-add
# contraction and no fast-math. guest.hpp also sets the pragmas Clang and MSVC
# honour; GCC only has the command-line switch, so link anything that compiles
# lifted code against this target.
add_library(qilin_runtime INTERFACE)
target_include_directories(qilin_runtime INTERFACE recomp/runtime)
if(MSVC)
    target_compile_options(qilin_runtime INTERFACE /fp:precise)
else()
    target_compile_options(qilin_runtime INTERFACE -ffp-contract=off -fno-fast-math)
endif()

# Developer tools on macOS: macOS 13.3 or later (std::format).
function(qilin_tool target)
    if(APPLE)
        target_compile_options(${target} PRIVATE -mmacosx-version-min=13.3)
        target_link_options(${target} PRIVATE -mmacosx-version-min=13.3)
    endif()
endfunction()

# ---- The XBE loader, the disc image reader and SHA-256: runtime code too (no std::format) ----
add_library(qilin_xbe STATIC recomp/src/xbe.cpp recomp/src/xiso.cpp recomp/src/sha256.cpp)
target_include_directories(qilin_xbe PUBLIC recomp/src recomp/runtime)
target_link_libraries(qilin_xbe PRIVATE qilin_warnings)

# ---- Recompiler core ----
add_library(qilin_core STATIC
    recomp/src/xbe_json.cpp
    recomp/src/decoder.cpp
    recomp/src/cfg.cpp
    recomp/src/vrange.cpp
    recomp/src/lifter.cpp
)
target_include_directories(qilin_core PUBLIC recomp/src)
target_link_libraries(qilin_core PUBLIC qilin_xbe Zydis qilin_runtime PRIVATE qilin_warnings)
qilin_tool(qilin_core)

add_executable(qilin recomp/src/main.cpp)
target_link_libraries(qilin PRIVATE qilin_core qilin_warnings)
qilin_tool(qilin)
target_compile_definitions(qilin PRIVATE QILIN_VERSION="${PROJECT_VERSION}")

# ---- Tests (no game files needed) ----
enable_testing()
add_executable(qilin_tests recomp/tests/tests.cpp)
target_link_libraries(qilin_tests PRIVATE qilin_core qilin_warnings)
qilin_tool(qilin_tests)
add_test(NAME qilin_tests COMMAND qilin_tests)

# The qilin tool itself, on the tests' synthetic XBE written out as a file.
set(QILIN_TEST_XBE ${CMAKE_BINARY_DIR}/synthetic.xbe)
add_test(NAME qilin_tool_write_xbe COMMAND qilin_tests --write-xbe ${QILIN_TEST_XBE})
set_tests_properties(qilin_tool_write_xbe PROPERTIES FIXTURES_SETUP qilin_xbe_file)
function(qilin_tool_test name expect)
    add_test(NAME qilin_tool_${name} COMMAND qilin ${ARGN})
    set_tests_properties(qilin_tool_${name} PROPERTIES FIXTURES_REQUIRED qilin_xbe_file PASS_REGULAR_EXPRESSION "${expect}")
endfunction()
qilin_tool_test(info "sha256: [0-9a-f]+" info ${QILIN_TEST_XBE})
qilin_tool_test(imports "AvGetSavedDataAddress" imports ${QILIN_TEST_XBE})
qilin_tool_test(cfg "functions" cfg ${QILIN_TEST_XBE})
qilin_tool_test(align "\\.text" align ${QILIN_TEST_XBE})
qilin_tool_test(lift "lifted [0-9]+ of [0-9]+ functions" lift ${QILIN_TEST_XBE} --out ${CMAKE_BINARY_DIR}/synthetic-gen)
# Unknown bytes in a lifted region are an error: the synthetic XBE has seven.
qilin_tool_test(coverage "unknown bytes in lifted regions: [1-9]" coverage ${QILIN_TEST_XBE} --list)
# Not an XBE and not a disc image: refused, with the reason.
qilin_tool_test(not_a_game "not an Xbox disc image" info ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)

# x87 transcendentals: fast paths against slow paths on each CI platform's compiler and CPU.
add_executable(qilin_x87_consistency tools/check/x87/consistency.cpp)
target_link_libraries(qilin_x87_consistency PRIVATE qilin_runtime qilin_warnings)
add_test(NAME qilin_x87_consistency COMMAND qilin_x87_consistency 200000)
