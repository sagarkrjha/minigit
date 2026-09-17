add_library(minigit_storage STATIC
    blob.cpp
    tree.cpp
    commit.cpp
    object_database.cpp
    object_parser.cpp
    hash_object.cpp
    cat_file.cpp
    ls_tree.cpp
)

target_include_directories(minigit_storage
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_storage
    PUBLIC
        minigit_core
        minigit_repository
)
