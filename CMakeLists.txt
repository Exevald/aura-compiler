cmake_minimum_required(VERSION 3.27)

if(APPLE AND (NOT DEFINED CMAKE_CXX_COMPILER OR CMAKE_CXX_COMPILER MATCHES "g\\+\\+"))
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE FILEPATH "C++ compiler" FORCE)
endif()

if(APPLE AND (NOT DEFINED CMAKE_OSX_SYSROOT OR CMAKE_OSX_SYSROOT STREQUAL ""))
    execute_process(
            COMMAND xcrun --sdk macosx --show-sdk-path
            OUTPUT_VARIABLE _aura_macos_sdk
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
    )
    if(_aura_macos_sdk)
        set(CMAKE_OSX_SYSROOT "${_aura_macos_sdk}" CACHE STRING "macOS SDK path" FORCE)
    endif()
endif()

project(aura-compiler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_subdirectory(src)
add_subdirectory(tests)

add_executable(
        aura-compiler
        main.cpp
)

target_link_libraries(aura-compiler PRIVATE
        compiler_lib
)
