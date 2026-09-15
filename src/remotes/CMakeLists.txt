add_library(minigit_remotes STATIC
    config.cpp
    transfer.cpp
    remote.cpp
    clone.cpp
    fetch.cpp
    push.cpp
    pull.cpp
)

target_include_directories(minigit_remotes
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_remotes
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
        minigit_staging
)
