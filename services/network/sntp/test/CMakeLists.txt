add_executable(services.network.sntp_test)
emil_build_for(services.network.sntp_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.sntp_test)

target_link_libraries(services.network.sntp_test PUBLIC
    gmock_main
    services.network.sntp
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.network.sntp_test PRIVATE
    TestSntpClient.cpp
)
