include(GoogleTest)

add_executable(
        integrational_tests
        Integrational_tests.cpp
)

target_include_directories(integrational_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/execution
)

target_link_libraries(integrational_tests PRIVATE
        gtest_main
        gmock
        vm_lib
)

gtest_discover_tests(integrational_tests)