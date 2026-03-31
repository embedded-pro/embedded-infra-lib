add_executable(services.network.echo_test)
emil_build_for(services.network.echo_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.echo_test)

target_link_libraries(services.network.echo_test PUBLIC
    gmock_main
    services.network.echo
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    services.echo_core_test_doubles
)

target_sources(services.network.echo_test PRIVATE
    TestEchoOnConnection.cpp
)
