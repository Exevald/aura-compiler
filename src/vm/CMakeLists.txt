include(FetchContent)
FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(
        vm_lib
        core/values/ValueHelper.cpp
        runtime/StringPool.cpp
        runtime/SharedRuntime.cpp
        runtime/ExecutionContext.cpp
        runtime/stdlib/MathModule.cpp
        runtime/stdlib/ArrayModule.cpp
        runtime/stdlib/StringModule.cpp
        runtime/stdlib/IOModule.cpp
        runtime/stdlib/LogModule.cpp
        runtime/stdlib/CoreModule.cpp
        runtime/stdlib/DiagnosticsModule.cpp
        runtime/stdlib/SyncModule.cpp
        execution/chunk/Chunk.cpp
        execution/VirtualMachineCallsAndActors.cpp
        execution/VirtualMachineCore.cpp
        execution/VirtualMachineDataOps.cpp
        execution/VirtualMachineDebug.cpp
)

target_include_directories(vm_lib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/execution
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)

add_subdirectory(tests)
