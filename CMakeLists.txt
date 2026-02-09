cmake_minimum_required(VERSION 3.27)
project(aura-compiler)

#if(APPLE)
#    set(LLVM_ROOT /opt/homebrew/opt/llvm)
#
#    set(CMAKE_C_COMPILER "${LLVM_ROOT}/bin/clang")
#    set(CMAKE_CXX_COMPILER "${LLVM_ROOT}/bin/clang++")
#
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -isystem ${LLVM_ROOT}/include/c++/v1")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -L${LLVM_ROOT}/lib -lc++abi")
#    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -L${LLVM_ROOT}/lib -lc++abi")
#    set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -L${LLVM_ROOT}/lib -lc++abi")
#
#endif()

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