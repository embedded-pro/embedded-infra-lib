#ifndef HAL_QEMU_CMSDK_CMSDK_TIMER_REGISTERS_HPP
#define HAL_QEMU_CMSDK_CMSDK_TIMER_REGISTERS_HPP

#include <cstddef>
#include <cstdint>

namespace hal
{
    struct CmsdkTimerRegisters
    {
        volatile uint32_t control;
        volatile uint32_t value;
        volatile uint32_t reload;
        volatile uint32_t intClear;
    };

    static_assert(offsetof(CmsdkTimerRegisters, intClear) == 0xc);

    constexpr uintptr_t cmsdkTimer0BaseAddress = 0x40000000;
    constexpr uintptr_t cmsdkTimer1BaseAddress = 0x40001000;
    constexpr int32_t cmsdkTimer0IrqNumber = 8;
    constexpr int32_t cmsdkTimer1IrqNumber = 9;
    constexpr uint32_t cmsdkTimerClockHz = 25000000u;

    constexpr uint32_t timerControlEnable = 1u << 0;
    constexpr uint32_t timerControlIntEnable = 1u << 3;
    constexpr uint32_t timerIntClear = 1u << 0;
}

#endif
