add_library(minigit_diff STATIC
    diff_engine.cpp
    diff.cpp
)

target_include_directories(minigit_diff
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_diff
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
