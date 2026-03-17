include(GoogleTest)

add_executable(
        ast_tests
        AST_tests.cpp
)

target_link_libraries(
        ast_tests PRIVATE
        gtest_main
        bytecode_lib
        ast_lib
        parser_lib
        vm_lib
)
gtest_discover_tests(ast_tests)