find_program(CARGO_EXECUTABLE cargo REQUIRED)

set(AURA_RUST_SYNC_DIR ${CMAKE_SOURCE_DIR}/src/vm/rust)
set(AURA_RUST_SYNC_LIB ${AURA_RUST_SYNC_DIR}/target/debug/libaura_sync_graph.a)

add_custom_command(
        OUTPUT ${AURA_RUST_SYNC_LIB}
        COMMAND ${CARGO_EXECUTABLE} build --manifest-path ${AURA_RUST_SYNC_DIR}/Cargo.toml
        WORKING_DIRECTORY ${AURA_RUST_SYNC_DIR}
        DEPENDS
                ${AURA_RUST_SYNC_DIR}/Cargo.toml
                ${AURA_RUST_SYNC_DIR}/src/lib.rs
)

add_custom_target(aura_sync_graph_rust DEPENDS ${AURA_RUST_SYNC_LIB})

add_library(aura_sync_graph STATIC IMPORTED GLOBAL)
set_target_properties(aura_sync_graph PROPERTIES
        IMPORTED_LOCATION ${AURA_RUST_SYNC_LIB}
)
add_dependencies(aura_sync_graph aura_sync_graph_rust)

add_library(
        ast_lib
        SymbolTable.cpp
        ASTBuilder.cpp
        SemanticAnalyzer.cpp
        SyncRustBridge.cpp
        SyncStaticAnalyzer.cpp
)

target_include_directories(ast_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ast_lib PUBLIC aura_sync_graph)
