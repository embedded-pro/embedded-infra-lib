add_executable(services.ble_profile_test)
emil_build_for(services.ble_profile_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.ble_profile_test)

target_link_libraries(services.ble_profile_test PUBLIC
    gmock_main
    services.ble_profile
    services.ble_profile_test_doubles
    services.ble_test_doubles
)

target_sources(services.ble_profile_test PRIVATE
    TestNordicUartCentral.cpp
    TestNordicUartPeripheral.cpp
)
