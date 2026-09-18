add_library(minigit_submodule STATIC
    submodule.cpp
    submodule_config.cpp
)

target_include_directories(minigit_submodule PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_submodule PUBLIC
    minigit_core
    minigit_storage
    minigit_staging
    minigit_repository
    minigit_branching
    minigit_history
    minigit_remotes
)
