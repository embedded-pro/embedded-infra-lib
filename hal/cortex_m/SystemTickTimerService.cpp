#include "hal/cortex_m/SystemTickTimerService.hpp"

namespace hal::cortex
{
    SystemTickTimerService::SystemTickTimerService(uint32_t coreClockHz,
        infra::Duration tickDuration,
        uint32_t id)
        : infra::TickOnInterruptTimerService(id, tickDuration)
        , systemTick(coreClockHz, tickDuration)
    {
        Register(sysTickIrq, InterruptPriority::lowest);
    }

    void SystemTickTimerService::Start()
    {
        systemTick.Enable();
    }

    void SystemTickTimerService::Stop()
    {
        systemTick.Disable();
    }

    void SystemTickTimerService::Invoke()
    {
        SystemTickInterrupt();
    }
}
