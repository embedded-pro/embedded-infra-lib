add_library(hal.qemu.cortex INTERFACE)

target_include_directories(hal.qemu.cortex INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_link_libraries(hal.qemu.cortex INTERFACE
    hal.cortex_m
    infra.event
    infra.timer
    hal.interfaces)
