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

# The inline assembly in this library produces diagnostics under strict settings that are
# not actionable.
set_target_properties(hal.cortex_m.runtime PROPERTIES COMPILE_WARNING_AS_ERROR Off)
