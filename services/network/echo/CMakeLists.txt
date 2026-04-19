add_library(services.network.echo ${EMIL_EXCLUDE_FROM_ALL} STATIC)

protocol_buffer_echo_cpp(services.network.echo Network.proto)
protocol_buffer_csharp(services.network.echo Network.proto)
protocol_buffer_java(services.network.echo Network.proto)

target_link_libraries(services.network.echo PUBLIC
    services.network.connection
    services.echo_core
    services.network.http
    services.network.websocket
    services.tracer
)

target_link_libraries(services.network.echo PRIVATE
    hal.generic
)

target_sources(services.network.echo PRIVATE
    EchoInstantiationOnNetwork.cpp
    EchoInstantiationOnNetwork.hpp
    EchoOnConnection.cpp
    EchoOnConnection.hpp
    Network.hpp
    StreamEcho.cpp
    StreamEcho.hpp
    TracingEchoOnConnection.hpp
)

add_subdirectory(test)
