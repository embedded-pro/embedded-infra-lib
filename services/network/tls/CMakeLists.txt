if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    add_library(services.network.tls ${EMIL_EXCLUDE_FROM_ALL} STATIC)

    target_link_libraries(services.network.tls PUBLIC
        services.network.connection
        services.network.echo
        services.tracer
    )

    if (NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
        target_link_libraries(services.network.tls PUBLIC
            ${EMIL_EXTERNAL_MBEDTLS_TARGET}
        )
    else()
        target_link_libraries(services.network.tls PUBLIC
            mbedtls
        )
    endif()

    target_sources(services.network.tls PRIVATE
        CertificatesMbedTls.cpp
        CertificatesMbedTls.hpp
        ConnectionMbedTls.cpp
        ConnectionMbedTls.hpp
        MbedTlsSession.cpp
        MbedTlsSession.hpp
        TracingConnectionMbedTls.cpp
        TracingConnectionMbedTls.hpp
    )

    if (EMIL_HOST_BUILD)
        target_sources(services.network.tls PRIVATE
            CertificateConvertor.cpp
            CertificateConvertor.hpp
        )
    endif()

    add_subdirectory(test)
    add_subdirectory(test_doubles)
endif()
