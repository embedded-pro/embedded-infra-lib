#include <cstdint>
#include <cstdlib>

extern "C"
{
    // Required by the C++ ABI for global object lifetime management.
    // Provided here because -nostartfiles suppresses crtbegin.o/crti.o.
    [[gnu::weak]] void* __dso_handle = nullptr;

    void _init()
    {}

    [[gnu::weak, noreturn]] void abort()
    {
        // Breaking is only safe once a debugger is attached to service it. Without one,
        // bkpt raises a debug fault, and aborting from inside a fault handler then
        // escalates that into lockup, losing the diagnostics that led here. ARMv6-M does
        // not expose DHCSR to software, so there the breakpoint is skipped entirely.
#if !defined(__ARM_ARCH_6M__)
        constexpr uintptr_t dhcsrAddr = 0xE000EDF0u;
        constexpr uint32_t dhcsrDebugen = 1u << 0;

        if ((*reinterpret_cast<volatile uint32_t*>(dhcsrAddr) & dhcsrDebugen) != 0)
            __asm volatile("bkpt 0");
#endif

        while (true)
        {}
    }

    [[gnu::weak]] void __assert_func(const char*, int, const char*, const char*)
    {
        std::abort();
    }
}
