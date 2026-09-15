add_library(minigit_cli STATIC
    dispatcher.cpp
)

target_include_directories(minigit_cli
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_cli
    PUBLIC
        minigit_core
        minigit_repository
        minigit_storage
        minigit_staging
        minigit_diff
        minigit_history
        minigit_branching
        minigit_merge
        minigit_stash
        minigit_remotes
)
