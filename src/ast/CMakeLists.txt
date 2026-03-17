add_library(
        ast_lib
        SymbolTable.cpp
        AST.cpp
)

target_include_directories(ast_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})