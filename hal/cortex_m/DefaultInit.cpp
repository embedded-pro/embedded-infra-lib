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
        // bkpt halts under a debugger; without one the processor continues past it,
        // so the loop is what actually stops execution.
        __asm volatile("bkpt 0");

        while (true)
        {}
    }

    [[gnu::weak]] void __assert_func(const char*, int, const char*, const char*)
    {
        std::abort();
    }
}
