add_library(services.network.connection_test_doubles STATIC)
emil_build_for(services.network.connection_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.network.connection_test_doubles PUBLIC
    gmock
    services.network.connection
)

target_sources(services.network.connection_test_doubles PRIVATE
    AddressMock.hpp
    ConnectionFactoryWithNameResolverStub.cpp
    ConnectionFactoryWithNameResolverStub.hpp
    ConnectionLoopBack.cpp
    ConnectionLoopBack.hpp
    ConnectionMock.cpp
    ConnectionMock.hpp
    ConnectionStub.cpp
    ConnectionStub.hpp
    DatagramMock.hpp
    MulticastMock.hpp
    NameResolverMock.hpp
    WiFiNetworkMock.hpp
)
