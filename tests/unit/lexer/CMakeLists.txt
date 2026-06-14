include(GoogleTest)

add_executable(
        lexer_unit_tests
        LexerGrammarFeatureTests.cpp
        LexerKeywordTests.cpp
        LexerLexemeTests.cpp
        LexerOperatorPunctuationTests.cpp
        LexerWhitespacePositionTests.cpp
        ReaderTests.cpp
)

target_link_libraries(lexer_unit_tests PRIVATE gtest_main lexer_lib)
gtest_discover_tests(lexer_unit_tests)
