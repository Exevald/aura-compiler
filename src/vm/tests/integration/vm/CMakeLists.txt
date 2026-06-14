include(GoogleTest)

add_executable(
        vm_integration_tests
        VMArithmeticIntegrationTests.cpp
        VMIteratorAlgorithmIntegrationTests.cpp
        ModuleImportSampleProgramIntegrationTests.cpp
        ModuleImportLanguageFeatureIntegrationTests.cpp
        ModuleImportBuiltinStdlibIntegrationTests.cpp
        ModuleImportCompiletimeSyncIntegrationTests.cpp
        ModuleImportMySqlIntegrationTests.cpp
)

target_include_directories(vm_integration_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/vm/execution
        ${CMAKE_SOURCE_DIR}/src/vm/tests/support
)

target_link_libraries(vm_integration_tests PRIVATE
        gtest_main
        gmock
        compiler_lib
)

gtest_discover_tests(vm_integration_tests)
