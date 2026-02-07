cmake_minimum_required(VERSION 3.27)
project(aura-compiler)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src)


add_executable(
        aura-compiler
        main.cpp
)