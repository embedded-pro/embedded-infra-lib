add_executable(drivers.eeprom_test)
emil_build_for(drivers.eeprom_test BOOL EMIL_BUILD_TESTS)
emil_add_test(drivers.eeprom_test)

target_link_libraries(drivers.eeprom_test PUBLIC
    gmock_main
    drivers.eeprom
    hal.interfaces_test_doubles
    infra.timer_test_helper
    infra.util_test_helper
)

target_sources(drivers.eeprom_test PRIVATE
    TestEeprom24xx.cpp
)
