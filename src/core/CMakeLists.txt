add_library(minigit_core STATIC
    file.cpp
    sha256.cpp
    zlib_compress.cpp
)

target_include_directories(minigit_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_core
    PUBLIC
        OpenSSL::Crypto
        ZLIB::ZLIB
)
