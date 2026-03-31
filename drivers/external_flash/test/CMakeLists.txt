add_executable(drivers.external_flash_test)
emil_build_for(drivers.external_flash_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.external_flash_test)

target_link_libraries(drivers.external_flash_test PUBLIC
    gmock_main
    drivers.external_flash
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(drivers.external_flash_test PRIVATE
    TestFlashQuadSpiCypressFll.cpp
    TestFlashQuadSpiMicronN25q.cpp
)
