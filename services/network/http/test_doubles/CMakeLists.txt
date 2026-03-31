add_library(services.network.http_test_doubles STATIC)
emil_build_for(services.network.http_test_doubles BOOL BUILD_TESTING)

target_link_libraries(services.network.http_test_doubles PUBLIC
    gmock
    services.network.http
)

target_sources(services.network.http_test_doubles PRIVATE
    HttpClientAuthenticationStub.hpp
    HttpClientMock.cpp
    HttpClientMock.hpp
    HttpRequestParserStub.hpp
    HttpServerMock.hpp
)
