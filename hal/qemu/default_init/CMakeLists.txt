add_library(hal.qemu.default_init STATIC)

emil_build_for(hal.qemu.default_init
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_include_directories(hal.qemu.default_init PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.default_init PRIVATE
    SystemInit.hpp
    SystemInit.cpp
    Startup.hpp
    Startup.cpp
    Atomic.cpp
    EventInfrastructure.hpp
    EventInfrastructure.cpp
    TracerOnSemihostingInfrastructure.hpp
    TracerOnSemihostingInfrastructure.cpp)

target_link_libraries(hal.qemu.default_init PUBLIC
    hal.qemu.cortex
    hal.qemu.sync
    services.tracer
    infra.event
    infra.timer)

target_link_options(hal.qemu.default_init INTERFACE
    -T "${CMAKE_CURRENT_SOURCE_DIR}/../linker/qemu_virt.ld"
    --specs=nosys.specs)

# Syscalls must land directly in the final ELF as object files, not archived,
# to avoid linker archive-ordering issues with newlib's syscall wrappers.
add_library(hal.qemu.syscalls OBJECT)

emil_build_for(hal.qemu.syscalls
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_sources(hal.qemu.syscalls PRIVATE
    Syscalls.cpp)

target_include_directories(hal.qemu.syscalls PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_link_libraries(hal.qemu.syscalls PUBLIC
    hal.qemu.cortex)
