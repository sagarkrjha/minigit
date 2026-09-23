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
    test_ls.cpp
    test_rebase.cpp
    test_pack.cpp
    test_worktree.cpp
    test_submodule.cpp
    test_bisect.cpp
    test_logger.cpp
    test_smart_http.cpp
    test_update.cpp
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
        minigit_worktree
        minigit_submodule
        minigit_bisect
        minigit_remotes
        minigit_update
        CURL::libcurl
)

if(WIN32)
    target_link_libraries(minigit_tests PRIVATE ws2_32)
endif()

add_test(NAME minigit_tests COMMAND minigit_tests)

add_executable(minigit_benchmarks
    benchmark_metrics.cpp
)

target_include_directories(minigit_benchmarks
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(minigit_benchmarks
    PRIVATE
        minigit_core
        minigit_storage
        minigit_diff
        minigit_repository
)

if(WIN32)
    target_link_libraries(minigit_benchmarks PRIVATE ws2_32 psapi)
endif()
