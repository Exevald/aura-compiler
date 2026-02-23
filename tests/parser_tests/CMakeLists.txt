include(GoogleTest)

add_executable(
        parser_tests
        Parser_tests.cpp
)

target_link_libraries(parser_tests PRIVATE GTest::GTest GTest::gtest_main lexer_lib parser_lib)
gtest_discover_tests(parser_tests)