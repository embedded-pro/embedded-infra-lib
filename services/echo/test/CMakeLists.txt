add_executable(services.echo_test)
emil_build_for(services.echo_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.echo_test)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
    target_compile_options(services.echo_test PRIVATE -Wno-braced-scalar-init)
endif()

target_link_libraries(services.echo_test PUBLIC
    gmock_main
    protobuf.test_doubles
    services.echo
    services.sesame_test_doubles
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    infra.event_test_helper
)

target_sources(services.echo_test PRIVATE
    TestEchoOnSesame.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestEchoPolicyDiffieHellman.cpp>
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestEchoPolicySymmetricKey.cpp>
)
