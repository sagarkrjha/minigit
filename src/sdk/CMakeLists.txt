add_library(minigit_sdk
    minigit_sdk.cpp
)

target_include_directories(minigit_sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_sdk
    PUBLIC
        minigit_core
        minigit_repository
        minigit_storage
        minigit_staging
        minigit_history
        minigit_branching
)
