include(GoogleTest)

add_executable(
        lexer_test
        Lexer_tests.cpp
        Reader_tests.cpp
)

target_link_libraries(lexer_test PRIVATE GTest::GTest GTest::gtest_main lexer_lib)
gtest_discover_tests(lexer_test)