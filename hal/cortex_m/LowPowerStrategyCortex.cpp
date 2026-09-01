#include "hal/cortex_m/LowPowerStrategyCortex.hpp"

namespace
{
    void Dsb()
    {
        __asm volatile("dsb" ::: "memory");
    }

    void Wfe()
    {
        __asm volatile("wfe");
    }

    void Sev()
    {
        __asm volatile("sev");
    }
}

namespace hal::cortex
{
    void LowPowerStrategyCortex::RequestExecution()
    {
        Dsb();
        Sev();
    }

    void LowPowerStrategyCortex::Idle(const infra::EventDispatcherWorker&)
    {
        Dsb();
        Wfe();
    }
}
