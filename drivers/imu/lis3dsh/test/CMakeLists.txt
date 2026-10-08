add_executable(drivers.imu.lis3dsh_test)
emil_build_for(drivers.imu.lis3dsh_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.imu.lis3dsh_test)

target_link_libraries(drivers.imu.lis3dsh_test PUBLIC
    gmock_main
    drivers.imu.lis3dsh
    hal.interfaces_test_doubles
    services.util_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.imu.lis3dsh_test PRIVATE
    TestLis3dshBusAccess.cpp
    TestLis3dshCore.cpp
)
