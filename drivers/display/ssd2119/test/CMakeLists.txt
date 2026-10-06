add_executable(drivers.display.ssd2119_test)
emil_build_for(drivers.display.ssd2119_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.display.ssd2119_test)

target_link_libraries(drivers.display.ssd2119_test PUBLIC
    gmock_main
    drivers.display.ssd2119
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.display.ssd2119_test PRIVATE
    TestSsd2119.cpp
    TestSsd2119BoostxlK350qvgS1.cpp
    TestSsd2119BusAccessSpi.cpp
)
