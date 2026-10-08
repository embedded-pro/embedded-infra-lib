add_executable(drivers.touch_screen.stmpe811_test)
emil_build_for(drivers.touch_screen.stmpe811_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.touch_screen.stmpe811_test)

target_link_libraries(drivers.touch_screen.stmpe811_test PUBLIC
    gmock_main
    drivers.touch_screen.stmpe811
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.touch_screen.stmpe811_test PRIVATE
    TestStmpe811.cpp
    TestStmpe811BusAccessI2c.cpp
)
