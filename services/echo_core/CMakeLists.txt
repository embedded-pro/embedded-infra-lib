add_library(services.echo_core ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.echo_core PUBLIC
    infra.syntax
    services.tracer
)

if (EMIL_ENABLE_GLOBAL_TRACING)
    target_compile_definitions(services.echo_core PUBLIC EMIL_ENABLE_GLOBAL_TRACING)
endif()

target_sources(services.echo_core PRIVATE
    Echo.cpp
    Echo.hpp
    EchoErrorPolicy.cpp
    EchoErrorPolicy.hpp
    EchoOnStreams.cpp
    EchoOnStreams.hpp
    Proto.hpp
    ProtoMessageReceiver.cpp
    ProtoMessageReceiver.hpp
    ProtoMessageSender.cpp
    ProtoMessageSender.hpp
    Serialization.cpp
    Serialization.hpp
    ServiceForwarder.cpp
    ServiceForwarder.hpp
    TracingEcho.cpp
    TracingEcho.hpp
)

add_subdirectory(test_doubles)

include(echo/protocol_buffer_echo)

if (EMIL_BUILD_TESTS)
    add_subdirectory(test)
endif()
