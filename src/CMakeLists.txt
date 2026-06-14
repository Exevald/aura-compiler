add_subdirectory(lexer)
add_subdirectory(parser)
add_subdirectory(rulesBuilder)
add_subdirectory(ast)
add_subdirectory(bytecode)
add_subdirectory(vm)
add_subdirectory(asmGenerator)

add_library(
        compiler_lib
        Compiler.cpp
        linker/PackageLinkerAst.cpp
        linker/PackageLinkerLoad.cpp
        linker/PackageLinkerResolve.cpp
)

target_include_directories(compiler_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
        compiler_lib PUBLIC
        bytecode_lib
        ast_lib
        parser_lib
        vm_lib
)
