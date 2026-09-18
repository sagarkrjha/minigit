add_library(minigit_worktree STATIC
    worktree.cpp
)

target_include_directories(minigit_worktree
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_worktree
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
