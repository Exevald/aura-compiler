include(GoogleTest)

add_executable(
        compiler_unit_tests
        CompilerBytecodeCoreTests.cpp
        CompilerBytecodeFeatureTests.cpp
        CompilerImportedTypesAndModulesTests.cpp
        CompilerSemanticCoreTests.cpp
        CompilerSemanticDataStructureTests.cpp
        CompilerSemanticFunctionAndEffectsTests.cpp
        CompilerSemanticGenericsTests.cpp
        CompilerModuleImportTests.cpp
        CompilerBuiltinStdlibTests.cpp
        CompilerSyncCompiletimeTests.cpp
        CompilerDatabaseBuiltinTests.cpp
)

target_include_directories(compiler_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
        compiler_unit_tests PRIVATE
        gtest_main
        compiler_lib
)
gtest_discover_tests(compiler_unit_tests DISCOVERY_TIMEOUT 30)
