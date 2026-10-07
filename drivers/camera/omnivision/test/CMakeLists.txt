add_executable(drivers.camera.omnivision_test)
emil_build_for(drivers.camera.omnivision_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.camera.omnivision_test)

target_link_libraries(drivers.camera.omnivision_test PUBLIC
    gmock_main
    drivers.camera.omnivision
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.camera.omnivision_test PRIVATE
    TestOmniVisionSensor.cpp
    TestRegisterTableRunner.cpp
    TestSccbBusAccessI2c.cpp
)
