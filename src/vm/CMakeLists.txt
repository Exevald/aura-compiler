cmake_minimum_required(VERSION 3.27)
project(vm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)

add_executable(
        vm
        main.cpp
        core/Value.cpp
        execution/ExecutionContext.cpp
        execution/VirtualMachine.cpp
        operations/OperationsHandler.cpp
)

add_library(
        vm_lib
        core/Value.cpp
        execution/ExecutionContext.cpp
        execution/VirtualMachine.cpp
        operations/OperationsHandler.cpp
)

add_subdirectory(tests)