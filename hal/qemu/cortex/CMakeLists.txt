add_library(hal.qemu.cortex STATIC)

emil_build_for(hal.qemu.cortex
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_include_directories(hal.qemu.cortex PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.cortex PRIVATE
    Semihosting.hpp
    Semihosting.cpp
    InterruptCortex.hpp
    InterruptCortex.cpp
    SystemTick.hpp
    SystemTick.cpp
    SystemTickTimerService.hpp
    SystemTickTimerService.cpp
    EventDispatcherCortex.hpp
    EventDispatcherCortex.cpp
    Reset.hpp
    Reset.cpp)

target_link_libraries(hal.qemu.cortex PUBLIC
    infra.event
    infra.timer
    hal.interfaces)
