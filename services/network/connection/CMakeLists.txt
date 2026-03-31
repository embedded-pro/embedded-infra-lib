add_library(services.network.connection ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.connection PUBLIC
    infra.syntax
    infra.timer
    hal.synchronous_interfaces
    services.tracer
    services.util
)

target_sources(services.network.connection PRIVATE
    Address.cpp
    Address.hpp
    Connection.cpp
    Connection.hpp
    ConnectionFactoryWithNameResolver.cpp
    ConnectionFactoryWithNameResolver.hpp
    ConnectionStatus.hpp
    Datagram.hpp
    ExclusiveConnection.cpp
    ExclusiveConnection.hpp
    ExclusiveStartingConnection.cpp
    ExclusiveStartingConnection.hpp
    Multicast.hpp
    NameResolver.hpp
    SingleConnectionListener.cpp
    SingleConnectionListener.hpp
    TracingConnectionFactoryWithNameResolver.cpp
    TracingConnectionFactoryWithNameResolver.hpp
    WiFiNetwork.cpp
    WiFiNetwork.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
