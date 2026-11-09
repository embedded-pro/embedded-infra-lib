add_library(hal.qemu.semihosting_test_doubles INTERFACE)
emil_build_for(hal.qemu.semihosting_test_doubles BOOL BUILD_TESTING)

target_link_libraries(hal.qemu.semihosting_test_doubles INTERFACE
    gmock
    hal.qemu.semihosting)

target_sources(hal.qemu.semihosting_test_doubles PRIVATE
    SemihostingFileSystemMock.hpp)
