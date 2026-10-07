add_executable(drivers.display.ili9341_test)
emil_build_for(drivers.display.ili9341_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.display.ili9341_test)

target_link_libraries(drivers.display.ili9341_test PUBLIC
    gmock_main
    drivers.display.ili9341
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.display.ili9341_test PRIVATE
    TestIli9341.cpp
    TestIli9341BusAccessSpi.cpp
)
