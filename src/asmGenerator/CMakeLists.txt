add_library(
        ast_gen_lib
        ASMGenerator.cpp
)

target_include_directories(ast_gen_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ast_gen_lib PRIVATE ast_lib)
