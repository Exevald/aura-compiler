include(GoogleTest)

add_executable(
        asm_unit_tests
        ASMGeneratorTests.cpp
)

target_include_directories(asm_unit_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(
        asm_unit_tests PRIVATE
        gtest_main
        ast_gen_lib
        ast_lib
        parser_lib
)

gtest_discover_tests(asm_unit_tests)
