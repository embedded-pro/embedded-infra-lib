add_executable(services.network.http_test)
emil_build_for(services.network.http_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.http_test)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
    target_compile_options(services.network.http_test PRIVATE -Wno-braced-scalar-init)
endif()

target_link_libraries(services.network.http_test PUBLIC
    gmock_main
    services.network.http
    services.network.http_test_doubles
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    hal.interfaces_test_doubles
)

target_sources(services.network.http_test PRIVATE
    TestHttpClient.cpp
    TestHttpClientAuthentication.cpp
    TestHttpClientBasic.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestHttpClientCachedConnection.cpp>
    TestHttpClientJson.cpp
    TestHttpServer.cpp
)
