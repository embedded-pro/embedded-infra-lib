add_executable(services.terminal_test)
emil_build_for(services.terminal_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.terminal_test)

target_link_libraries(services.terminal_test PUBLIC
    gmock_main
    services.terminal
)

target_sources(services.terminal_test PRIVATE
    TestTerminalTypes.cpp
    TestVt100Parser.cpp
    TestTerminalScreen.cpp
    TestVt100Terminal.cpp
)
