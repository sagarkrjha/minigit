add_library(minigit_branching STATIC
    branch.cpp
    checkout.cpp
    switch_branch.cpp
    tag.cpp
)

target_include_directories(minigit_branching
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_branching
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
