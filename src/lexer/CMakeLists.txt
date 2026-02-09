add_library(
        lexer_lib
        utils/Utils.cpp
        reader/Reader.cpp
        rules/Whitespace.cpp
        rules/Identifier.cpp
        rules/Number.cpp
        rules/String.cpp
        rules/Operator.cpp
        Lexer.cpp
)

target_include_directories(lexer_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})