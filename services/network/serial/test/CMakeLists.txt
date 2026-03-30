add_executable(services.network.serial_test)
emil_build_for(services.network.serial_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.serial_test)

target_link_libraries(services.network.serial_test PUBLIC
    gmock_main
    services.network.serial
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    hal.interfaces_test_doubles
)

target_sources(services.network.serial_test PRIVATE
    TestSerialServer.cpp
)
