add_executable(minigit_tests
    test_main.cpp
    test_core.cpp
    test_storage.cpp
    test_staging.cpp
    test_diff.cpp
    test_repository.cpp
    test_cherry_pick.cpp
    test_show.cpp
    test_clean.cpp
)

target_include_directories(minigit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(minigit_tests
    PRIVATE
        minigit_core
        minigit_storage
        minigit_staging
        minigit_diff
        minigit_repository
        minigit_merge
        minigit_history
)

add_test(NAME minigit_tests COMMAND minigit_tests)
