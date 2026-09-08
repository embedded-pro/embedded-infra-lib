add_executable(emil.services.terminal.test)
emil_build_for(emil.services.terminal.test BOOL EMIL_BUILD_TESTS)
emil_add_test(emil.services.terminal.test)

target_link_libraries(emil.services.terminal.test PUBLIC
    gmock_main
    emil.services.terminal
)

target_sources(emil.services.terminal.test PRIVATE
    TestTerminalTypes.cpp
    TestVt100Parser.cpp
    TestTerminalScreen.cpp
    TestVt100Terminal.cpp
)
