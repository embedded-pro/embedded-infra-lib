add_library(services.network.mqtt_test_doubles INTERFACE)
emil_build_for(services.network.mqtt_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.network.mqtt_test_doubles INTERFACE
    gmock
    services.network.mqtt
)

target_sources(services.network.mqtt_test_doubles INTERFACE
    MqttMock.hpp
)
