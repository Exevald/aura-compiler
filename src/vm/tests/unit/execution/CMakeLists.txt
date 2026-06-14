include(GoogleTest)

add_executable(
        vm_execution_unit_tests
        Chunk_tests.cpp
        Function_tests.cpp
        Variables_tests.cpp
        VirtualMachineActorTests.cpp
        VirtualMachineArithmeticControlTests.cpp
        VirtualMachineCoreExecutionTests.cpp
        VirtualMachineDiagnosticsModuleTests.cpp
        VirtualMachineStdlibTests.cpp
        VirtualMachineSyncModuleTests.cpp
)

target_include_directories(vm_execution_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/vm
        ${CMAKE_SOURCE_DIR}/src/vm/execution
        ${CMAKE_SOURCE_DIR}/src/vm/runtime
        ${CMAKE_SOURCE_DIR}/src/vm/core
        ${CMAKE_SOURCE_DIR}/src/vm/tests/support
)

target_link_libraries(vm_execution_unit_tests PRIVATE
        gtest_main
        gmock
        vm_lib
)

gtest_discover_tests(vm_execution_unit_tests)
