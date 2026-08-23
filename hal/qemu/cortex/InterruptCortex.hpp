#ifndef HAL_QEMU_CORTEX_INTERRUPT_CORTEX_HPP
#define HAL_QEMU_CORTEX_INTERRUPT_CORTEX_HPP

#include <cstdint>

namespace hal::cortex
{
    class InterruptHandler
    {
    public:
        virtual void Invoke() = 0;

    protected:
        ~InterruptHandler() = default;
    };

    void InterruptCortexRegisterHandler(int32_t irqNumber, InterruptHandler& handler);
    void InterruptCortexDispatch(int32_t irqNumber);
    void EnableInterrupt(int32_t irqNumber);
    void DisableInterrupt(int32_t irqNumber);
    void SetInterruptPriority(int32_t irqNumber, uint8_t priority);
}

#endif
