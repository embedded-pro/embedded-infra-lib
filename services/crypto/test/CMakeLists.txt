add_executable(services.crypto_test)
emil_build_for(services.crypto_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.crypto_test)

target_link_libraries(services.crypto_test PUBLIC
    gmock_main
    services.crypto
    services.crypto_test_doubles
)

target_sources(services.crypto_test PRIVATE
    TestAesCmac.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestAesMbedTls.cpp>
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestEllipticCurveDiffieHellman.cpp>
    TestKeyGenerator.cpp
)
