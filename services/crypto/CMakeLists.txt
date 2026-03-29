add_library(services.crypto ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.crypto PUBLIC
    infra.util
)

target_sources(services.crypto PRIVATE
    Aes.hpp
    AesCmac.cpp
    AesCmac.hpp
    EllipticCurve.hpp
    EllipticCurveDiffieHellman.cpp
    EllipticCurveDiffieHellman.hpp
    KeyGenerator.cpp
    KeyGenerator.hpp
    Secp256r1.cpp
    Secp256r1.hpp
    Sha256.hpp
)

if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    if (NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
        target_link_libraries(services.crypto PUBLIC
            ${EMIL_EXTERNAL_MBEDTLS_TARGET}
        )
    else()
        target_link_libraries(services.crypto PUBLIC
            mbedcrypto
        )
    endif()

    target_compile_definitions(services.crypto PUBLIC
        EMIL_USE_MBEDTLS
    )

    target_sources(services.crypto PRIVATE
        AesMbedTls.cpp
        AesMbedTls.hpp
        MbedTlsRandomDataGeneratorWrapper.cpp
        MbedTlsRandomDataGeneratorWrapper.hpp
        Sha256MbedTls.cpp
        Sha256MbedTls.hpp
    )
endif()

add_subdirectory(test)
add_subdirectory(test_doubles)
