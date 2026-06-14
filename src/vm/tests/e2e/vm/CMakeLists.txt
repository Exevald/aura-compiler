include(GoogleTest)

add_executable(
        vm_e2e_tests
        ModuleImportMySqlE2ETests.cpp
)

target_include_directories(vm_e2e_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/vm/execution
        ${CMAKE_SOURCE_DIR}/src/vm/tests/support
)

target_link_libraries(vm_e2e_tests PRIVATE
        gtest_main
        gmock
        compiler_lib
)

gtest_discover_tests(vm_e2e_tests)
