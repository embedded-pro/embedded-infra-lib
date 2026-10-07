add_executable(drivers.audio.wm8994_test)
emil_build_for(drivers.audio.wm8994_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.audio.wm8994_test)

target_link_libraries(drivers.audio.wm8994_test PUBLIC
    gmock_main
    drivers.audio.wm8994
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.audio.wm8994_test PRIVATE
    TestWm8994.cpp
    TestWm8994BusAccessI2c.cpp
    Wm8994BusMock.hpp
)
