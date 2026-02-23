add_library(
        rules_builder_lib
        RulesBuilder.cpp
)

target_include_directories(rules_builder_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})