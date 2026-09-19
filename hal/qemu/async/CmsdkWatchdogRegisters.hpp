#ifndef HAL_QEMU_ASYNC_CMSDK_WATCHDOG_REGISTERS_HPP
#define HAL_QEMU_ASYNC_CMSDK_WATCHDOG_REGISTERS_HPP

#include <cstddef>
#include <cstdint>

namespace hal
{
    struct CmsdkWatchdogRegisters
    {
        volatile uint32_t load;
        volatile uint32_t value;
        volatile uint32_t control;
        volatile uint32_t intClr;
        volatile uint32_t ris;
        volatile uint32_t mis;
        volatile uint32_t reserved[762];
        volatile uint32_t lock;
    };

    static_assert(offsetof(CmsdkWatchdogRegisters, lock) == 0xc00);

    // CMSDK APB watchdog on the QEMU MPS2 machines (mps2-an385/an386/an500), clocked from SYSCLK
    constexpr uintptr_t cmsdkWatchdogBaseAddress = 0x40008000;
    constexpr uint32_t cmsdkWatchdogClockHz = 25000000u;

    constexpr uint32_t watchdogControlIntEnable = 1u << 0;
    constexpr uint32_t watchdogControlResetEnable = 1u << 1;
    constexpr uint32_t watchdogRisTimeout = 1u << 0;
    constexpr uint32_t watchdogLockUnlockKey = 0x1acce551u;
}

#endif
