include(GoogleTest)

add_executable(
        core_tests
        Value_tests.cpp
)

target_include_directories(core_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(core_tests PRIVATE
        GTest::gtest_main
        GTest::gtest
        vm_lib
)

gtest_discover_tests(core_tests)