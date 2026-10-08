add_executable(drivers.imu.lis302dl_test)
emil_build_for(drivers.imu.lis302dl_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.lis302dl_test)

target_link_libraries(drivers.imu.lis302dl_test PUBLIC
    gmock_main
    drivers.imu.lis302dl
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.lis302dl_test PRIVATE
    TestLis302dlBusAccess.cpp
    TestLis302dlCore.cpp
)
