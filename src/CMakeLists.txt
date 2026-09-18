add_subdirectory(core)
add_subdirectory(repository)
add_subdirectory(storage)
add_subdirectory(staging)
add_subdirectory(diff)
add_subdirectory(history)
add_subdirectory(branching)
add_subdirectory(merge)
add_subdirectory(stash)
add_subdirectory(remotes)
add_subdirectory(worktree)
add_subdirectory(cli)

add_executable(minigit
    main.cpp
)

target_link_libraries(minigit
    PRIVATE
        minigit_cli
)
