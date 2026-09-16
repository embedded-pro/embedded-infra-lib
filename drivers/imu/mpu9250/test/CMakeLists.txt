add_executable(drivers.imu.mpu9250_test)
emil_build_for(drivers.imu.mpu9250_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.mpu9250_test)

target_link_libraries(drivers.imu.mpu9250_test PUBLIC
    gmock_main
    drivers.imu.mpu9250
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.mpu9250_test PRIVATE
    TestMpu9250BusAccess.cpp
)
