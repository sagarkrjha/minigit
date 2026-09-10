cmake_minimum_required(VERSION 3.20)

project(minigit
    VERSION 0.1.0
    LANGUAGES CXX    
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)

add_executable(
    minigit
    src/main.cpp

    # commands
    src/commands/init.cpp
    src/commands/hash_object.cpp
    src/commands/add.cpp
    src/commands/write_tree.cpp
    src/commands/commit.cpp
    src/commands/log.cpp
    src/commands/status.cpp
    src/commands/diff.cpp
    src/commands/branch.cpp
    src/commands/checkout.cpp
    src/commands/switch_branch.cpp

    # domain objects
    src/objects/blob.cpp
    src/objects/tree.cpp
    src/objects/commit.cpp
    src/objects/object_database.cpp
    src/objects/object_parser.cpp

    # supporting modules
    src/repository/repository.cpp
    src/index/index.cpp
    src/hashing/sha256.cpp
    src/filesystem/file.cpp
    src/diff/diff.cpp
)

target_link_libraries(
    minigit
    PRIVATE
    OpenSSL::Crypto
)