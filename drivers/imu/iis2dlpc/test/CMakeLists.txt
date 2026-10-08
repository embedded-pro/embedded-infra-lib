add_executable(drivers.imu.iis2dlpc_test)
emil_build_for(drivers.imu.iis2dlpc_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.iis2dlpc_test)

target_link_libraries(drivers.imu.iis2dlpc_test PUBLIC
    gmock_main
    drivers.imu.iis2dlpc
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.iis2dlpc_test PRIVATE
    TestIis2dlpcBusAccess.cpp
    TestIis2dlpcCore.cpp
)
