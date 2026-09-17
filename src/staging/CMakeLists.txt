add_library(minigit_staging STATIC
    index.cpp
    ignore.cpp
    add.cpp
    status.cpp
    reset.cpp
    write_tree.cpp
    clean.cpp
)

target_include_directories(minigit_staging
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_staging
    PUBLIC
        minigit_core
        minigit_storage
        minigit_repository
)
