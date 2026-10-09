add_executable(drivers.touch_screen.ft6x06_test)
emil_build_for(drivers.touch_screen.ft6x06_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.touch_screen.ft6x06_test)

target_link_libraries(drivers.touch_screen.ft6x06_test PUBLIC
    gmock_main
    drivers.touch_screen.ft6x06
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.touch_screen.ft6x06_test PRIVATE
    TestFt6x06.cpp
    TestFt6x06BusAccessI2c.cpp
)
