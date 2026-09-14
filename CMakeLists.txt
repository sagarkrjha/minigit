cmake_minimum_required(VERSION 3.20)

project(minigit
    VERSION 1.0.0
    LANGUAGES CXX    
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(
    minigit
    src/main.cpp

    # commands
    src/commands/init.cpp
    src/commands/hash_object.cpp
    src/commands/cat_file.cpp
    src/commands/add.cpp
    src/commands/write_tree.cpp
    src/commands/commit.cpp
    src/commands/log.cpp
    src/commands/status.cpp
    src/commands/diff.cpp
    src/commands/branch.cpp
    src/commands/checkout.cpp
    src/commands/switch_branch.cpp
    src/commands/reset.cpp
    src/commands/tag.cpp
    src/commands/merge.cpp
    src/commands/revert.cpp
    src/commands/stash.cpp
    src/commands/remote.cpp
    src/commands/clone.cpp
    src/commands/fetch.cpp
    src/commands/push.cpp
    src/commands/pull.cpp

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
    src/ignore/ignore.cpp
    src/merge/merge_engine.cpp
    src/compression/zlib_compress.cpp
    src/remotes/config.cpp
    src/remotes/transfer.cpp
)

target_link_libraries(
    minigit
    PRIVATE
    OpenSSL::Crypto
    ZLIB::ZLIB
)