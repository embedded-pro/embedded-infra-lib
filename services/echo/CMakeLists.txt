add_library(services.echo ${EMIL_EXCLUDE_FROM_ALL} STATIC)

target_link_libraries(services.echo PUBLIC
    hal.interfaces
    infra.syntax
    infra.timer
    protobuf.echo
    services.message_communication
    services.sesame
)

# Ensure protobuf generation in services.sesame completes before
# compiling echo sources that include generated headers
add_dependencies(services.echo services.sesame)

target_sources(services.echo PRIVATE
    EchoInstantiation.cpp
    EchoInstantiation.hpp
    EchoOnMessageCommunication.cpp
    EchoOnMessageCommunication.hpp
    EchoOnSesame.cpp
    EchoOnSesame.hpp
    EchoPolicyDiffieHellman.cpp
    EchoPolicyDiffieHellman.hpp
    EchoPolicySymmetricKey.cpp
    EchoPolicySymmetricKey.hpp
    TracingEchoOnSesame.hpp
)

if (EMIL_INCLUDE_MBEDTLS OR NOT EMIL_EXTERNAL_MBEDTLS_TARGET STREQUAL "")
    target_sources(services.echo PRIVATE
        EchoInstantiationSecured.cpp
        EchoInstantiationSecured.hpp
    )
endif()

add_subdirectory(test)
