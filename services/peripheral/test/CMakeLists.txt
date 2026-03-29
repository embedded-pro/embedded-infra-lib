add_executable(services.peripheral_test)
emil_build_for(services.peripheral_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.peripheral_test)

target_link_libraries(services.peripheral_test PUBLIC
    gmock_main
    services.peripheral
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
)

target_sources(services.peripheral_test PRIVATE
    TestDebouncedButton.cpp
    TestDebugLed.cpp
    TestI2cMultipleAccess.cpp
    TestInverseLogicPin.cpp
    TestLowPowerSerialCommunication.cpp
    TestLowPowerSpiMaster.cpp
    TestRepeatingButton.cpp
    TestSerialCommunicationLoopback.cpp
    TestSignalLed.cpp
    TestSpiMasterWithChipSelect.cpp
    TestSpiMultipleAccess.cpp
)
