add_executable(services.synchronous_flash_test)
emil_build_for(services.synchronous_flash_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.synchronous_flash_test)

target_link_libraries(services.synchronous_flash_test PUBLIC
    gmock_main
    hal.synchronous_interfaces_test_doubles
    services.synchronous_flash
)

target_sources(services.synchronous_flash_test PRIVATE
    TestSynchronousFlashSpi.cpp
)
