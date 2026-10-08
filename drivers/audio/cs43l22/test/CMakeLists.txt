add_executable(drivers.audio.cs43l22_test)
emil_build_for(drivers.audio.cs43l22_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.audio.cs43l22_test)

target_link_libraries(drivers.audio.cs43l22_test PUBLIC
    gmock_main
    drivers.audio.cs43l22
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.audio.cs43l22_test PRIVATE
    TestCs43l22BusAccessI2c.cpp
)
