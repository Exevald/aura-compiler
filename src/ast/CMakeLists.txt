add_library(
        ast_lib
        SymbolTable.cpp
        builder/CoreASTBuilder.cpp
        builder/DeclarationsASTBuilder.cpp
        builder/FlattenASTBuilder.cpp
        builder/StatementsASTBuilder.cpp
        sematic/CallAndTypesSemanticAnalyzer.cpp
        sematic/CoreAnalysisSemanticAnalyzer.cpp
        sematic/DeclarationsSemanticAnalyzer.cpp
        sematic/EnvironmentSemanticAnalyzer.cpp
        sematic/ExpressionsSemanticAnalyzer.cpp
        sematic/StatementsSemanticAnalyzer.cpp
        SyncGraphAnalyzer.cpp
        SyncStaticAnalyzer.cpp
)

target_include_directories(ast_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
