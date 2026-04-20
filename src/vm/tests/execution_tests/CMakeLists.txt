include(GoogleTest)

add_executable(
        execution_tests
        Chunk_tests.cpp
        ExecutionContext_tests.cpp
        SharedRuntime_tests.cpp
        VirtualMachine_tests.cpp
        detail/VirtualMachineActorTests.cpp
        detail/VirtualMachineArithmeticControlTests.cpp
        detail/VirtualMachineCoreExecutionTests.cpp
        detail/VirtualMachineDiagnosticsModuleTests.cpp
        detail/VirtualMachineStdlibTests.cpp
        detail/VirtualMachineSyncModuleTests.cpp
        Function_tests.cpp
)

target_include_directories(execution_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/vm
        ${CMAKE_SOURCE_DIR}/src/vm/execution
        ${CMAKE_SOURCE_DIR}/src/vm/runtime
        ${CMAKE_SOURCE_DIR}/src/vm/core
)

target_link_libraries(execution_tests PRIVATE
        gtest_main
        gmock
        vm_lib
)

gtest_discover_tests(execution_tests)
