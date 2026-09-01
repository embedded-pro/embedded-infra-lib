add_library(hal.cortex_m STATIC)

emil_build_for(hal.cortex_m
    BOOL EMIL_BUILD_CORTEX_M
    HOST Off)

target_include_directories(hal.cortex_m PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>")

target_sources(hal.cortex_m PRIVATE
    DataWatchpointAndTrace.hpp
    DataWatchpointAndTrace.cpp
    EventDispatcherCortex.hpp
    EventDispatcherCortex.cpp
    FaultTracer.hpp
    FaultTracer.cpp
    InterruptCortex.hpp
    InterruptCortex.cpp
    LowPowerStrategyCortex.hpp
    LowPowerStrategyCortex.cpp
    LowPriorityInterrupt.hpp
    LowPriorityInterrupt.cpp
    SystemTick.hpp
    SystemTick.cpp
    SystemTickTimerService.hpp
    SystemTickTimerService.cpp
    Reset.hpp
    Reset.cpp
    Semihosting.hpp
    Semihosting.cpp)

target_link_libraries(hal.cortex_m PUBLIC
    hal.cortex_m.runtime
    hal.interfaces
    infra.event
    infra.timer
    infra.util
    services.tracer)

set_target_properties(hal.cortex_m PROPERTIES COMPILE_WARNING_AS_ERROR Off)

add_library(hal.cortex_m.runtime OBJECT)

emil_build_for(hal.cortex_m.runtime
    BOOL EMIL_BUILD_CORTEX_M
    HOST Off)

target_include_directories(hal.cortex_m.runtime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>")

target_sources(hal.cortex_m.runtime PRIVATE
    Atomic.cpp
    DefaultInit.cpp
    SysCallStubs.cpp)

set_target_properties(hal.cortex_m.runtime PROPERTIES COMPILE_WARNING_AS_ERROR Off)
