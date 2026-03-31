add_executable(services.sesame_test)
emil_build_for(services.sesame_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.sesame_test)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
    target_compile_options(services.sesame_test PRIVATE -Wno-braced-scalar-init)
endif()

target_link_libraries(services.sesame_test PUBLIC
    gmock_main
    services.echo_core_test_doubles
    services.sesame
    services.sesame_test_doubles
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    infra.event_test_helper
)

target_sources(services.sesame_test PRIVATE
    TestSesameCobs.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestSesameSecured.cpp>
    TestSesameWindowed.cpp
)
