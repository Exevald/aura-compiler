include(GoogleTest)

add_executable(
        compiler_tests
        CompilerBytecodeCoreTests.cpp
        CompilerBytecodeFeatureTests.cpp
        CompilerImportedTypesAndModulesTests.cpp
        CompilerSemanticAndGenericsTests.cpp
        CompilerStdlibAndSyncTests.cpp
        ${CMAKE_SOURCE_DIR}/src/Compiler.cpp
)

target_link_libraries(
        compiler_tests PRIVATE
        gtest_main
        bytecode_lib
        ast_lib
        parser_lib
        vm_lib
)
gtest_discover_tests(compiler_tests)
