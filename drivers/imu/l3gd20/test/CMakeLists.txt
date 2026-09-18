add_executable(drivers.imu.l3gd20_test)
emil_build_for(drivers.imu.l3gd20_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.l3gd20_test)

target_link_libraries(drivers.imu.l3gd20_test PUBLIC
    gmock_main
    drivers.imu.l3gd20
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.l3gd20_test PRIVATE
    TestL3gd20BusAccess.cpp
    TestL3gd20Core.cpp
)
