#ifndef HAL_CORTEX_M_LOW_POWER_STRATEGY_CORTEX_HPP
#define HAL_CORTEX_M_LOW_POWER_STRATEGY_CORTEX_HPP

#include "infra/event/LowPowerEventDispatcher.hpp"

namespace hal::cortex
{
    class LowPowerStrategyCortex
        : public infra::LowPowerStrategy
    {
    public:
        void RequestExecution() override;
        void Idle(const infra::EventDispatcherWorker& eventDispatcher) override;
    };
}

#endif
