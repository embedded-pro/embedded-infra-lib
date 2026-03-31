add_library(services.network.http ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.network.http PUBLIC
    services.network.connection
    services.crypto
    services.tracer
)

target_sources(services.network.http PRIVATE
    Http.cpp
    Http.hpp
    HttpClient.hpp
    HttpClientAuthentication.cpp
    HttpClientAuthentication.hpp
    HttpClientBasic.cpp
    HttpClientBasic.hpp
    HttpClientCachedConnection.cpp
    HttpClientCachedConnection.hpp
    HttpClientImpl.cpp
    HttpClientImpl.hpp
    HttpClientJson.cpp
    HttpClientJson.hpp
    HttpErrors.cpp
    HttpErrors.hpp
    HttpRequestParser.cpp
    HttpRequestParser.hpp
    HttpServer.cpp
    HttpServer.hpp
    TracingHttpClientCachedConnection.cpp
    TracingHttpClientCachedConnection.hpp
    TracingHttpClientImpl.cpp
    TracingHttpClientImpl.hpp
    TracingStatusHttpClientImpl.cpp
    TracingStatusHttpClientImpl.hpp
)

add_subdirectory(test)
add_subdirectory(test_doubles)
