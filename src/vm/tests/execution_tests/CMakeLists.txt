include(GoogleTest)

add_executable(
        execution_tests
        Chunk_tests.cpp
        ExecutionContext_tests.cpp
)

target_include_directories(execution_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/execution
)

target_link_libraries(execution_tests PRIVATE
        gtest_main
        vm_lib
)

gtest_discover_tests(execution_tests)