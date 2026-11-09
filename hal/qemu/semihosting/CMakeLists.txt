add_library(hal.qemu.semihosting STATIC)

emil_build_for(hal.qemu.semihosting
    HOST All
    BOOL EMIL_BUILD_QEMU)

target_include_directories(hal.qemu.semihosting PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.semihosting PRIVATE
    SemihostingAsyncFlash.hpp
    SemihostingAsyncFlash.cpp
    SemihostingEeprom.hpp
    SemihostingEeprom.cpp
    SemihostingFileFlash.hpp
    SemihostingFileFlash.cpp
    SemihostingFileSystem.hpp
    SemihostingRecordFile.hpp
    SemihostingRecordFile.cpp)

target_link_libraries(hal.qemu.semihosting PUBLIC
    hal.interfaces
    infra.event
    infra.util
    services.synchronous_flash)

add_library(hal.qemu.semihosting_cortex STATIC)

emil_build_for(hal.qemu.semihosting_cortex
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_sources(hal.qemu.semihosting_cortex PRIVATE
    SemihostingFileSystemCortex.hpp
    SemihostingFileSystemCortex.cpp)

target_link_libraries(hal.qemu.semihosting_cortex PUBLIC
    hal.cortex_m
    hal.qemu.semihosting)

add_subdirectory(test_doubles)
add_subdirectory(test)
