add_executable(hal.qemu.test)

emil_build_for(hal.qemu.test
    BOOL EMIL_BUILD_QEMU)

target_sources(hal.qemu.test PRIVATE
    TestSemihostingWriter.cpp
    TestSystemTickTimerService.cpp)

target_link_libraries(hal.qemu.test PRIVATE
    hal.cortex_m.runtime
    hal.qemu.syscalls
    hal.qemu.default_init
    hal.qemu.sync
    hal.qemu.cortex
    infra.event
    infra.timer
    infra.event_test_helper
    gmock_main)

emil_add_test(hal.qemu.test)
