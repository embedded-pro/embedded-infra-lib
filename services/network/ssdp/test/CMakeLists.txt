add_executable(services.network.ssdp_test)
emil_build_for(services.network.ssdp_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.ssdp_test)

target_link_libraries(services.network.ssdp_test PUBLIC
    gmock_main
    services.network.ssdp
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.network.ssdp_test PRIVATE
    TestSsdpDeviceDiscovery.cpp
)
