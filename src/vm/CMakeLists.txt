cmake_minimum_required(VERSION 3.27)
project(vm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
        vm
        main.cpp
        core/Value.cpp
        execution/ExecutionContext.cpp
        execution/Chunk.cpp
        execution/VirtualMachine.cpp
        operations/OperationsHandler.cpp
)

add_library(
        vm_lib
        core/Value.cpp
        execution/ExecutionContext.cpp
        execution/Chunk.cpp
        execution/VirtualMachine.cpp
        operations/OperationsHandler.cpp
)

add_subdirectory(tests)