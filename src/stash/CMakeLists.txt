add_library(minigit_stash STATIC
    stash.cpp
)

target_include_directories(minigit_stash
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_stash
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
