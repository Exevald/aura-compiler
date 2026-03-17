add_library(
        parser_lib
        Parser.cpp
        TableBuilder.cpp
)

target_include_directories(parser_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(parser_lib PRIVATE lexer_lib rules_builder_lib ast_lib bytecode_lib)