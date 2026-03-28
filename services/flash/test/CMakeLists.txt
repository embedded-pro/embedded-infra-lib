add_executable(services.flash_test)
emil_build_for(services.flash_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.flash_test)

target_link_libraries(services.flash_test PUBLIC
    gmock_main
    services.flash
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(services.flash_test PRIVATE
    TestFlashAlign.cpp
    TestFlashMultipleAccess.cpp
    TestFlashQuadSpiCypressFll.cpp
    TestFlashQuadSpiMicronN25q.cpp
    TestFlashQuadSpiSingleSpeed.cpp
    TestFlashRegion.cpp
    TestFlashSpi.cpp
)
