add_library(services.sesame ${EMIL_EXCLUDE_FROM_ALL} STATIC)

protocol_buffer_echo_cpp(services.sesame SesameSecurity.proto)

if (EMIL_ENABLE_TRACING)
    target_compile_definitions(services.sesame PUBLIC EMIL_ENABLE_TRACING=1)
else()
    target_compile_definitions(services.sesame PUBLIC EMIL_DISABLE_TRACING=1)
endif()

target_link_libraries(services.sesame PUBLIC
    hal.interfaces
    infra.syntax
    infra.timer
    services.echo_core
    services.crypto
)

target_sources(services.sesame PRIVATE
    Sesame.hpp
    SesameCobs.cpp
    SesameCobs.hpp
    SesameCrypto.hpp
    SesameSecured.cpp
    SesameSecured.hpp
    SesameWindowed.cpp
    SesameWindowed.hpp
)

if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    if (NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
        target_link_libraries(services.sesame PUBLIC
            ${EMIL_EXTERNAL_MBEDTLS_TARGET}
        )
    else()
        target_link_libraries(services.sesame PUBLIC
            mbedcrypto
            mbedx509
        )
    endif()

    target_compile_definitions(services.sesame PUBLIC
        EMIL_USE_MBEDTLS
    )

    target_sources(services.sesame PRIVATE
        SesameCryptoMbedTls.cpp
        SesameCryptoMbedTls.hpp
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
