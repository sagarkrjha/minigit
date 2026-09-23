add_library(minigit_install STATIC
    install.cpp
)

target_include_directories(minigit_install
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(minigit_install
    PUBLIC
        minigit_core
)

if(WIN32)
    target_link_libraries(minigit_install
        PUBLIC
            shell32
            advapi32
            user32
    )
endif()
