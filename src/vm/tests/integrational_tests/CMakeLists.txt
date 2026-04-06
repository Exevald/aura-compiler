include(GoogleTest)

add_executable(
        integrational_tests
        ${CMAKE_SOURCE_DIR}/src/Compiler.cpp
        Integrational_tests.cpp
        ModuleImport_tests.cpp
        Variables_tests.cpp
)

target_include_directories(integrational_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/vm/execution
)

target_link_libraries(integrational_tests PRIVATE
        gtest_main
        gmock
        bytecode_lib
        ast_lib
        parser_lib
        vm_lib
)

gtest_discover_tests(integrational_tests)
