include(GoogleTest)

add_executable(
        vm_core_unit_tests
        Value_tests.cpp
)

target_include_directories(vm_core_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src/vm
        ${CMAKE_SOURCE_DIR}/src/vm/core
)

target_link_libraries(vm_core_unit_tests PRIVATE
        gtest_main
        vm_lib
)

gtest_discover_tests(vm_core_unit_tests)
