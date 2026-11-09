add_executable(hal.qemu.semihosting_test)
emil_build_for(hal.qemu.semihosting_test BOOL EMIL_BUILD_TESTS)
emil_add_test(hal.qemu.semihosting_test)

target_link_libraries(hal.qemu.semihosting_test PUBLIC
    gmock_main
    hal.qemu.semihosting
    hal.qemu.semihosting_test_doubles
    infra.event_test_helper)

target_sources(hal.qemu.semihosting_test PRIVATE
    TestSemihostingAsyncFlash.cpp
    TestSemihostingEeprom.cpp
    TestSemihostingFileFlash.cpp
    TestSemihostingRecordFile.cpp)
