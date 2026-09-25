#include "hal/cortex_m/LowPowerModeCortex.hpp"
#include <cstdint>

namespace
{
    constexpr uintptr_t scbScr = 0xE000ED10u;
    constexpr uint32_t scrSleepDeep = 1u << 2;

    volatile uint32_t& Scr()
    {
        return *reinterpret_cast<volatile uint32_t*>(scbScr);
    }
}

namespace hal::cortex
{
    void LowPowerModeCortex::Enter(PowerMode mode)
    {
        WaitForInterrupt(false);
    }

    void WaitForInterrupt(bool deepSleep)
    {
        if (deepSleep)
            Scr() |= scrSleepDeep;
        else
            Scr() &= ~scrSleepDeep;

        __asm volatile("dsb" ::: "memory");
        __asm volatile("wfi");
        __asm volatile("isb");

        Scr() &= ~scrSleepDeep;
    }
}
