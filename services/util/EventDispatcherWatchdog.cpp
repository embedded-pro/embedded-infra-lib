#include "services/util/EventDispatcherWatchdog.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace
{
    uint32_t ToNumberOfPeriods(infra::Duration expirationTimeout, infra::Duration period)
    {
        really_assert(period > infra::Duration::zero());
        auto count = (expirationTimeout + period - infra::Duration(1)) / period;
        return static_cast<uint32_t>(std::max<infra::Duration::rep>(count, 1));
    }
}

namespace services
{
    EventDispatcherWatchdog::EventDispatcherWatchdog(hal::WatchdogWithEarlyWarning& watchdog, const infra::Function<void()>& onExpired, const Config& config)
        : watchdog(watchdog)
        , progress(infra::EventDispatcher::Instance().Progress())
        , expirationCount(ToNumberOfPeriods(config.expirationTimeout, watchdog.EarlyWarningPeriod()))
        , stepsAtLastEarlyWarning(progress.Steps())
        , onExpired(onExpired)
    {
        watchdog.Start([this]()
            {
                EarlyWarning();
            });
    }

    void EventDispatcherWatchdog::Refresh()
    {
        watchdog.Refresh();
    }

    bool EventDispatcherWatchdog::EventDispatcherProgressed()
    {
        auto steps = progress.Steps();
        auto progressed = !infra::ExecutionProgress::IsExecuting(steps) || steps != stepsAtLastEarlyWarning;
        stepsAtLastEarlyWarning = steps;
        return progressed;
    }

    void EventDispatcherWatchdog::EarlyWarning()
    {
        // Once expired the watchdog is no longer refreshed, so the hardware resets the device if onExpired returns
        if (expired)
            return;

        watchdog.Refresh();

        if (EventDispatcherProgressed())
            missedEarlyWarnings = 0;
        else if (++missedEarlyWarnings == expirationCount)
        {
            expired = true;
            onExpired();
        }
    }
}
