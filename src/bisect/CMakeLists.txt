add_library(minigit_bisect STATIC
    bisect.cpp
)

target_include_directories(minigit_bisect PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_bisect PUBLIC
    minigit_core
    minigit_storage
    minigit_staging
    minigit_repository
    minigit_branching
    minigit_history
    minigit_diff
)
