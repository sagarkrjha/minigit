add_library(minigit_merge STATIC
    merge_engine.cpp
    merge.cpp
    revert.cpp
    cherry_pick.cpp
)

target_include_directories(minigit_merge
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_merge
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
        minigit_diff
)
