#ifndef HAL_CORTEX_M_LOW_PRIORITY_INTERRUPT_HPP
#define HAL_CORTEX_M_LOW_PRIORITY_INTERRUPT_HPP

#include "hal/cortex_m/InterruptCortex.hpp"
#include "infra/util/Function.hpp"

namespace hal::cortex
{
    class LowPriorityInterrupt
        : private InterruptHandler
    {
    public:
        void Trigger();
        void Register(const infra::Function<void()>& handler);
        void Unregister();

    private:
        void Invoke() override;

    private:
        infra::Function<void()> onInvoke;
    };
}

#endif
