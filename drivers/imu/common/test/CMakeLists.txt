add_executable(drivers.imu.common_test)
emil_build_for(drivers.imu.common_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.common_test)

target_link_libraries(drivers.imu.common_test PUBLIC
    gmock_main
    drivers.imu.common
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.common_test PRIVATE
    ImuTestSensor.hpp
    TestAccelerometerSensor.cpp
    TestImuBusAccessI2c.cpp
    TestImuBusAccessSpi.cpp
    TestRegisterSensor.cpp
    TestSensorWithPolling.cpp
)
