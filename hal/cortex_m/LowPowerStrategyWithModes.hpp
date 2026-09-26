#ifndef HAL_CORTEX_M_LOW_POWER_STRATEGY_WITH_MODES_HPP
#define HAL_CORTEX_M_LOW_POWER_STRATEGY_WITH_MODES_HPP

#include "hal/interfaces/LowPowerMode.hpp"
#include "infra/event/LowPowerEventDispatcher.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace hal::cortex
{
    class LowPowerStrategyWithModes;

    class DeepSleepObserver
        : public infra::Observer<DeepSleepObserver, LowPowerStrategyWithModes>
    {
    public:
        using infra::Observer<DeepSleepObserver, LowPowerStrategyWithModes>::Observer;

        virtual void EnteringDeepSleep() = 0;
        virtual void LeftDeepSleep() = 0;
    };

    class LowPowerStrategyWithModes
        : public infra::LowPowerStrategy
        , public infra::Subject<DeepSleepObserver>
    {
    public:
        LowPowerStrategyWithModes(LowPowerMode& lowPowerMode, const infra::MainClockReference& mainClock, uint32_t timerServiceId = infra::systemTimerServiceId);

        void RequestExecution() override;
        void Idle(const infra::EventDispatcherWorker& eventDispatcher) override;

    private:
        bool DeepSleepAllowed() const;
        void DeepSleep();

        LowPowerMode& lowPowerMode;
        const infra::MainClockReference& mainClock;
        uint32_t timerServiceId;
    };
}

#endif
