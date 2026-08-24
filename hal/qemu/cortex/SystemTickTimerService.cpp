#include "hal/qemu/cortex/SystemTickTimerService.hpp"

namespace hal::cortex
{
    SystemTickTimerService::SystemTickTimerService(uint32_t coreClockHz,
        infra::Duration tickDuration,
        uint32_t id)
        : infra::TickOnInterruptTimerService(id, tickDuration)
        , systemTick(coreClockHz, tickDuration)
    {
        Register(sysTickIrq, InterruptPriority::lowest);
        systemTick.Enable();
    }

    void SystemTickTimerService::Invoke()
    {
        SystemTickInterrupt();
    }
}
