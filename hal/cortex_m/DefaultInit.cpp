#include <cstdint>
#include <cstdlib>

extern "C"
{
    [[gnu::weak]] void* __dso_handle = nullptr;

    void _init()
    {}

    [[gnu::weak, noreturn]] void abort()
    {
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
