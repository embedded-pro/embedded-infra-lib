add_library(hal.qemu.async STATIC)

emil_build_for(hal.qemu.async
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_include_directories(hal.qemu.async PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.async PRIVATE
    CmsdkWatchdogRegisters.hpp
    UartQemu.hpp
    UartQemu.cpp
    WatchdogQemu.hpp
    WatchdogQemu.cpp)

target_link_libraries(hal.qemu.async PUBLIC
    hal.qemu.cortex
    hal.interfaces
    infra.event
    infra.timer)
