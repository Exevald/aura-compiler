include(GoogleTest)

add_executable(
        lexer_tests
        Lexer_tests.cpp
        Reader_tests.cpp
)

target_link_libraries(lexer_tests PRIVATE gtest_main lexer_lib)
gtest_discover_tests(lexer_tests)