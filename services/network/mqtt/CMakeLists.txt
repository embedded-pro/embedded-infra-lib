add_library(services.network.mqtt ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.mqtt PUBLIC
    services.network.connection
    services.tracer
)

target_sources(services.network.mqtt PRIVATE
    Mqtt.hpp
    MqttClientImpl.cpp
    MqttClientImpl.hpp
    MqttMultipleAccess.cpp
    MqttMultipleAccess.hpp
    TracingMqttClientImpl.cpp
    TracingMqttClientImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
