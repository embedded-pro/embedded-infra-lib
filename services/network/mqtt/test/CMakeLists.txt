add_executable(services.network.mqtt_test)
emil_build_for(services.network.mqtt_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.mqtt_test)

target_link_libraries(services.network.mqtt_test PUBLIC
    gmock_main
    services.network.mqtt
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.network.mqtt_test PRIVATE
    TestMqttClient.cpp
)
