add_library(hal.cortex_m STATIC)

emil_build_for(hal.cortex_m
    BOOL EMIL_BUILD_CORTEX_M
    HOST Off)

target_include_directories(hal.cortex_m PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>")

target_sources(hal.cortex_m PRIVATE
    InterruptCortex.hpp
    InterruptCortex.cpp)

target_link_libraries(hal.cortex_m PUBLIC
    hal.cortex_m.runtime
    infra.event
    infra.util)

# The naked functions and inline assembly in this library produce diagnostics under
# strict settings that are not actionable.
set_target_properties(hal.cortex_m PROPERTIES COMPILE_WARNING_AS_ERROR Off)

# The compiler runtime, ABI and syscall symbols must reach the final ELF as loose object
# files. Archived, they are only extracted once something already references them, which
# happens too late to beat the definitions newlib pulls out of libg_nano.a; the wide
# character stubs in particular lose to libc_a-svfwprintf.o and drag in its unresolved
# __ssputws_r. This mirrors hal.qemu.syscalls, which exists for the same reason.
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
