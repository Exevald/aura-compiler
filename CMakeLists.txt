cmake_minimum_required(VERSION 3.27)
project(aura-compiler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(GTest REQUIRED)

add_subdirectory(src)
add_subdirectory(tests)

add_executable(
        aura-compiler
        main.cpp
)

target_link_libraries(aura-compiler PRIVATE lexer_lib)