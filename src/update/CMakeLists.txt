add_library(minigit_update STATIC
    json.cpp
    semver.cpp
    update.cpp
    notifier.cpp
)

target_include_directories(minigit_update
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_update
    PUBLIC
        minigit_core
        minigit_remotes
        minigit_repository
        CURL::libcurl
)
