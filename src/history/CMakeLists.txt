add_library(minigit_history STATIC
    commit.cpp
    log.cpp
)

target_include_directories(minigit_history
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_history
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
