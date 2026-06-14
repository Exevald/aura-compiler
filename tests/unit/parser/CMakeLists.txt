include(GoogleTest)

add_executable(
        parser_unit_tests
        ParserTests.cpp
)

target_link_libraries(parser_unit_tests PRIVATE gtest_main lexer_lib parser_lib ast_lib)
gtest_discover_tests(parser_unit_tests)
