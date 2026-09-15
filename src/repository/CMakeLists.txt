add_library(minigit_repository STATIC
    repository.cpp
    init.cpp
)

target_include_directories(minigit_repository
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_repository
    PUBLIC
        minigit_core
)
