if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    add_executable(services.network.tls_test)
    emil_build_for(services.network.tls_test BOOL EMIL_BUILD_TESTS)
    emil_add_test(services.network.tls_test)

    if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
        target_compile_options(services.network.tls_test PRIVATE -Wno-braced-scalar-init)
    endif()

    target_link_libraries(services.network.tls_test PUBLIC
        gmock_main
        services.network.tls
        services.network.tls_test_doubles
        services.network.connection_test_doubles
        infra.timer_test_helper
        infra.util_test_helper
        hal.generic
    )

    target_sources(services.network.tls_test PRIVATE
        TestCertificateConvertor.cpp
        TestCertificatesMbedTls.cpp
        TestConnectionMbedTls.cpp
    )
endif()
