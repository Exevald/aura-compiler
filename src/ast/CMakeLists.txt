add_library(
        ast_lib
        SymbolTable.cpp
        ASTBuilder.cpp
        SemanticAnalyzer.cpp
)

target_include_directories(ast_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})