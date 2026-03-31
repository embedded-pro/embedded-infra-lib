add_library(services.network.sntp_test_doubles INTERFACE)
emil_build_for(services.network.sntp_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.network.sntp_test_doubles INTERFACE
    gmock
    services.network.sntp
)

target_sources(services.network.sntp_test_doubles INTERFACE
    SntpMock.hpp
)
