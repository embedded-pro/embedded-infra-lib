add_executable(boards.mb1166_test)
emil_build_for(boards.mb1166_test BOOL EMIL_BUILD_TESTS)
emil_add_test(boards.mb1166_test)

target_link_libraries(boards.mb1166_test PUBLIC
    gmock_main
    boards.mb1166
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(boards.mb1166_test PRIVATE
    TestMb1166Setup.cpp
)
