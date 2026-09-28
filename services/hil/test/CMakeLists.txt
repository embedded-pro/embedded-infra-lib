add_executable(services.hil_test)
emil_build_for(services.hil_test BOOL EMIL_BUILD_TESTS)
emil_add_test(services.hil_test)

target_link_libraries(services.hil_test PUBLIC
    gmock_main
    services.hil
    services.hil.commands
    hal.interfaces_test_doubles
    hal.synchronous_interfaces_test_doubles
    infra.timer_test_helper
    infra.event_test_helper
    infra.util_test_helper
)

target_sources(services.hil_test PRIVATE
    HilFixture.hpp
    TestHilAdcCommands.cpp
    TestHilArguments.cpp
    TestHilCanCommands.cpp
    TestHilCommand.cpp
    TestHilComparatorCommands.cpp
    TestHilEepromCommands.cpp
    TestHilEthernetCommands.cpp
    TestHilGpioCommands.cpp
    TestHilPendingOperation.cpp
    TestHilPinNaming.cpp
    TestHilPinPool.cpp
    TestHilPwmCommands.cpp
    TestHilQeiCommands.cpp
    TestHilResponse.cpp
    TestHilSpiCommands.cpp
    TestHilSystemCommands.cpp
    TestHilTerminal.cpp
    TestHilUartCommands.cpp
    TestHilUnsupportedCommands.cpp
    TestHilWatchDogCommands.cpp
)
