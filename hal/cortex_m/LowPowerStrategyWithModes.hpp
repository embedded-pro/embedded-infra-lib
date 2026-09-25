#ifndef HAL_CORTEX_M_LOW_POWER_STRATEGY_WITH_MODES_HPP
#define HAL_CORTEX_M_LOW_POWER_STRATEGY_WITH_MODES_HPP

#include "hal/interfaces/LowPowerMode.hpp"
#include "infra/event/LowPowerEventDispatcher.hpp"
#include "infra/timer/Timer.hpp"
#include <cstdint>

namespace hal::cortex
{
    // Enters deep sleep only when no peripheral holds the main clock and no timer is pending,
    // because the system tick stops in deep sleep and would delay every pending timer
    class LowPowerStrategyWithModes
        : public infra::LowPowerStrategy
    {
    public:
        LowPowerStrategyWithModes(LowPowerMode& lowPowerMode, const infra::MainClockReference& mainClock, uint32_t timerServiceId = infra::systemTimerServiceId);

        void RequestExecution() override;
        void Idle(const infra::EventDispatcherWorker& eventDispatcher) override;

    private:
        PowerMode SelectMode() const;

        LowPowerMode& lowPowerMode;
        const infra::MainClockReference& mainClock;
        uint32_t timerServiceId;
    };
}

#endif
