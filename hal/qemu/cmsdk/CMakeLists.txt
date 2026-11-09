add_library(hal.qemu.cmsdk STATIC)

emil_build_for(hal.qemu.cmsdk
    HOST All
    BOOL EMIL_BUILD_QEMU)

target_include_directories(hal.qemu.cmsdk PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../..>")

target_sources(hal.qemu.cmsdk PRIVATE
    CmsdkCycleCounter.hpp
    CmsdkCycleCounter.cpp
    CmsdkLineUart.hpp
    CmsdkLineUart.cpp
    CmsdkTimerRegisters.hpp
    CmsdkUartRegisters.hpp
    LineAssembler.hpp
    LineAssembler.cpp)

target_link_libraries(hal.qemu.cmsdk PUBLIC
    infra.util)

add_library(hal.qemu.cmsdk_cortex STATIC)

emil_build_for(hal.qemu.cmsdk_cortex
    BOOL EMIL_BUILD_QEMU
    HOST Off)

target_sources(hal.qemu.cmsdk_cortex PRIVATE
    CmsdkTimer.hpp
    CmsdkTimer.cpp)

target_link_libraries(hal.qemu.cmsdk_cortex PUBLIC
    hal.cortex_m
    hal.qemu.cmsdk)

add_subdirectory(test)
