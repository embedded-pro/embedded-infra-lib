add_executable(hal.qemu.cmsdk_test)
emil_build_for(hal.qemu.cmsdk_test BOOL EMIL_BUILD_TESTS)
emil_add_test(hal.qemu.cmsdk_test)

target_link_libraries(hal.qemu.cmsdk_test PUBLIC
    gmock_main
    hal.qemu.cmsdk)

target_sources(hal.qemu.cmsdk_test PRIVATE
    TestCmsdkCycleCounter.cpp
    TestCmsdkLineUart.cpp
    TestLineAssembler.cpp)
