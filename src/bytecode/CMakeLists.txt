add_library(
        bytecode_lib
        BytecodeGenerator.cpp
)

target_include_directories(bytecode_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(bytecode_lib PRIVATE ast_lib)