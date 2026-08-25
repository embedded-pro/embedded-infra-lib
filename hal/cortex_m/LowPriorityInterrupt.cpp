#include "hal/cortex_m/LowPriorityInterrupt.hpp"

namespace
{
    constexpr uintptr_t scbIcsr = 0xE000ED04u;
    constexpr uint32_t scbIcsrPendSvSet = 1u << 28;

    volatile uint32_t& Reg32(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }
}

namespace hal::cortex
{
    void LowPriorityInterrupt::Trigger()
    {
        Reg32(scbIcsr) = scbIcsrPendSvSet;
    }

    void LowPriorityInterrupt::Register(const infra::Function<void()>& handler)
    {
        onInvoke = handler;
        InterruptHandler::Register(pendSvIrq, InterruptPriority::lowest);
    }

    void LowPriorityInterrupt::Unregister()
    {
        InterruptHandler::Unregister();
        onInvoke = nullptr;
    }

    void LowPriorityInterrupt::Invoke()
    {
        if (onInvoke)
            onInvoke();
    }
}
