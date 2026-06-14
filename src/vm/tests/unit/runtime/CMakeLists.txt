include(GoogleTest)

add_executable(
        vm_runtime_unit_tests
        SharedRuntime_tests.cpp
        ExecutionContextStateTests.cpp
        ExecutionContextTransactionTests.cpp
)

target_include_directories(vm_runtime_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/vm
        ${CMAKE_SOURCE_DIR}/src/vm/runtime
        ${CMAKE_SOURCE_DIR}/src/vm/core
)

target_link_libraries(vm_runtime_unit_tests PRIVATE
        gtest_main
        gmock
        vm_lib
)

gtest_discover_tests(vm_runtime_unit_tests)
