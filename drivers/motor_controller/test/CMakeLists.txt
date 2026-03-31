add_executable(drivers.motor_controller_test)
emil_build_for(drivers.motor_controller_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.motor_controller_test)

target_link_libraries(drivers.motor_controller_test PUBLIC
    gmock_main
    drivers.motor_controller
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.motor_controller_test PRIVATE
    TestStepperMotorControllerDrv8711.cpp
)
