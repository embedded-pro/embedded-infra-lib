add_executable(services.util_test)
emil_build_for(services.util_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.util_test)

if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang|AppleClang")
    target_compile_options(services.util_test PRIVATE -Wno-braced-scalar-init)
endif()

target_link_libraries(services.util_test PUBLIC
    gmock_main
    services.tracer
    services.util
    services.util_test_doubles
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
    infra.event_test_helper
)

target_sources(services.util_test PRIVATE
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestConfigurationStore.cpp>
    TestCyclicStore.cpp
    TestTerminal.cpp
    TestTerminalWithStorage.cpp
    TestTimeWithLocalization.cpp
    $<$<BOOL:${EMIL_INCLUDE_MBEDTLS}>:TestWritableConfiguration.cpp>
)
