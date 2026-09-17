add_executable(drivers.imu.lsm303dlhc_test)
emil_build_for(drivers.imu.lsm303dlhc_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.lsm303dlhc_test)

target_link_libraries(drivers.imu.lsm303dlhc_test PUBLIC
    gmock_main
    drivers.imu.lsm303dlhc
    hal.interfaces_test_doubles
    services.peripheral
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.lsm303dlhc_test PRIVATE
    TestLsm303dlhc.cpp
    TestLsm303dlhcAccelerometer.cpp
    TestLsm303dlhcBusAccess.cpp
    TestLsm303dlhcLifetime.cpp
    TestLsm303dlhcMagnetometer.cpp
    TestLsm303dlhcMixins.cpp
)
