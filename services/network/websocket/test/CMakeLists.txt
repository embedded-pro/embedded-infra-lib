add_executable(services.network.websocket_test)
emil_build_for(services.network.websocket_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.websocket_test)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
    target_compile_options(services.network.websocket_test PRIVATE -Wno-braced-scalar-init)
endif()

target_link_libraries(services.network.websocket_test PUBLIC
    gmock_main
    services.network.websocket
    services.network.connection_test_doubles
    services.network.http_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    hal.interfaces_test_doubles
    hal.synchronous_interfaces_test_doubles
)

target_sources(services.network.websocket_test PRIVATE
    TestWebSocketClient.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestWebSocketClientConnectionObserver.cpp>
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestWebSocketIntegration.cpp>
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestWebSocketServerConnectionObserver.cpp>
)
