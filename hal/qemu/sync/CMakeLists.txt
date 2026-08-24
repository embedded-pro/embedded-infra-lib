add_library(hal.qemu.sync STATIC)

emil_build_for(hal.qemu.sync
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_include_directories(hal.qemu.sync PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.sync PRIVATE
    Pl011Registers.hpp
    SemihostingWriter.hpp
    SemihostingWriter.cpp
    SynchronousUartQemu.hpp
    SynchronousUartQemu.cpp)

target_link_libraries(hal.qemu.sync PUBLIC
    hal.qemu.cortex
    hal.synchronous_interfaces
    infra.stream)
