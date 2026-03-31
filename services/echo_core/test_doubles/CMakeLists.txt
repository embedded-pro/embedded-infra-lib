add_library(services.echo_core_test_doubles STATIC)
emil_build_for(services.echo_core_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.echo_core_test_doubles PUBLIC
    gmock
    services.echo_core
)

target_sources(services.echo_core_test_doubles PRIVATE
    EchoSingleLoopback.cpp
    EchoSingleLoopback.hpp
    EchoMock.hpp
    ServiceStub.cpp
    ServiceStub.hpp
)
