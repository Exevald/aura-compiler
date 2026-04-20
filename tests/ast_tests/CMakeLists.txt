include(GoogleTest)

add_executable(
        ast_tests
        utils/ASTSearcher.cpp
        ASTCoreStructureTests.cpp
        ASTInterfacesGenericsTests.cpp
        ASTStructEnumTests.cpp
        ASTTypesAndControlTests.cpp
)

target_link_libraries(
        ast_tests PRIVATE
        gtest_main
        bytecode_lib
        lexer_lib
        ast_lib
        parser_lib
        vm_lib
)
gtest_discover_tests(ast_tests)