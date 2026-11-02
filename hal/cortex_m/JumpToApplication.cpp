#include "hal/cortex_m/JumpToApplication.hpp"

namespace hal::cortex
{
    namespace
    {
        constexpr uintptr_t vtorAddress = 0xE000ED08u;
    }

    void JumpToApplication(uint32_t vectorTableAddress)
    {
        const auto* vectors = reinterpret_cast<const uint32_t*>(vectorTableAddress);
        const auto stackPointer = vectors[0];
        const auto resetHandler = vectors[1];

        *reinterpret_cast<volatile uint32_t*>(vtorAddress) = vectorTableAddress;
        asm volatile("dsb\n"
                     "isb\n"
            :
            :
            : "memory");

        // One asm block: once the stack pointer moves, nothing may be read back from the old stack
        asm volatile("msr msp, %0\n"
                     "bx %1\n"
            :
            : "r"(stackPointer), "r"(resetHandler)
            : "memory");
        __builtin_unreachable();
    }
}
