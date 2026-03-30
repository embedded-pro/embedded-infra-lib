add_executable(services.network.connection_test)
emil_build_for(services.network.connection_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.connection_test)

target_link_libraries(services.network.connection_test PUBLIC
    gmock_main
    services.network.connection
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.network.connection_test PRIVATE
    TestAddress.cpp
    TestExclusiveConnection.cpp
    TestExclusiveStartingConnection.cpp
    TestSingleConnectionListener.cpp
    TestWiFiNetwork.cpp
)
