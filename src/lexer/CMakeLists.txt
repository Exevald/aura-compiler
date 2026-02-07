add_library(
        lexer_lib
        reader/Reader.cpp
        Lexer.cpp
)

target_include_directories(lexer_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})