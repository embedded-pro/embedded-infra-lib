#include "services/util/EventDispatcherWatchdog.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace
{
    uint32_t EarlyWarningsUntilExpiry(infra::Duration expirationTimeout, infra::Duration earlyWarningPeriod)
    {
        really_assert(earlyWarningPeriod > infra::Duration::zero());
        auto count = (expirationTimeout + earlyWarningPeriod - infra::Duration(1)) / earlyWarningPeriod;
        return static_cast<uint32_t>(std::max<infra::Duration::rep>(count, 1));
    }
}

namespace services
{
    EventDispatcherWatchdogSupervision::EventDispatcherWatchdogSupervision(hal::Watchdog& watchdog, infra::Duration expirationTimeout, const infra::Function<void()>& onExpired)
        : watchdog(watchdog)
        , expirationCount(EarlyWarningsUntilExpiry(expirationTimeout, watchdog.EarlyWarningPeriod()))
        , onExpired(onExpired)
    {
        watchdog.Start([this]()
            {
                EarlyWarning();
            });
    }

    void EventDispatcherWatchdogSupervision::Step()
    {
        steps.store(steps.load() + 1);
    }

    bool EventDispatcherWatchdogSupervision::Progressed()
    {
        auto current = steps.load();
        auto progressed = current % 2 == 0 || current != stepsAtLastEarlyWarning;
        stepsAtLastEarlyWarning = current;
        return progressed;
    }

    void EventDispatcherWatchdogSupervision::EarlyWarning()
    {
        if (expired)
            return;

        watchdog.Refresh();

        if (Progressed())
            missedEarlyWarnings = 0;
        else if (++missedEarlyWarnings == expirationCount)
        {
            expired = true;
            onExpired();
        }
    }

    template class EventDispatcherWatchdogWorker<infra::EventDispatcherWorkerImpl>;
    template class EventDispatcherWatchdogWorker<infra::EventDispatcherWithWeakPtrWorker>;
    template class EventDispatcherWatchdogWorker<infra::LowPowerEventDispatcherWorker>;
}
