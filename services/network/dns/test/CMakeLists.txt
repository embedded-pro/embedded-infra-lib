add_executable(services.network.dns_test)
emil_build_for(services.network.dns_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.network.dns_test)

target_link_libraries(services.network.dns_test PUBLIC
    gmock_main
    services.network.dns
    services.network.connection_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(services.network.dns_test PRIVATE
    TestBonjourServer.cpp
    TestDnsResolver.cpp
    TestDnsServer.cpp
    TestLlmnrResponder.cpp
    TestMdnsClient.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestNameResolverCache.cpp>
)
