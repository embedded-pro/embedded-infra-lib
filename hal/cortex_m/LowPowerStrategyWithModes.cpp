#include "hal/cortex_m/LowPowerStrategyWithModes.hpp"
#include "infra/timer/TimerService.hpp"

namespace
{
    class InterruptsMasked
    {
    public:
        InterruptsMasked()
        {
            __asm volatile("mrs %0, primask" : "=r"(primask));
            __asm volatile("cpsid i" ::: "memory");
        }

        InterruptsMasked(const InterruptsMasked&) = delete;
        InterruptsMasked& operator=(const InterruptsMasked&) = delete;

        ~InterruptsMasked()
        {
            __asm volatile("msr primask, %0" ::"r"(primask) : "memory");
        }

    private:
        uint32_t primask;
    };
}

namespace hal::cortex
{
    LowPowerStrategyWithModes::LowPowerStrategyWithModes(LowPowerMode& lowPowerMode, const infra::MainClockReference& mainClock, uint32_t timerServiceId)
        : lowPowerMode(lowPowerMode)
        , mainClock(mainClock)
        , timerServiceId(timerServiceId)
    {}

    void LowPowerStrategyWithModes::RequestExecution()
    {}

    void LowPowerStrategyWithModes::Idle(const infra::EventDispatcherWorker& eventDispatcher)
    {
        InterruptsMasked interruptsMasked;

        if (!eventDispatcher.IsIdle())
            return;

        if (DeepSleepAllowed())
            DeepSleep();
        else
            lowPowerMode.Enter(PowerMode::sleep);
    }

    bool LowPowerStrategyWithModes::DeepSleepAllowed() const
    {
        return !mainClock.IsReferenced() && infra::TimerService::GetTimerService(timerServiceId).NextTrigger() == infra::TimePoint::max();
    }

    void LowPowerStrategyWithModes::DeepSleep()
    {
        NotifyObservers([](auto& observer)
            {
                observer.EnteringDeepSleep();
            });

        lowPowerMode.Enter(PowerMode::deepSleep);

        NotifyObservers([](auto& observer)
            {
                observer.LeftDeepSleep();
            });
    }
}
