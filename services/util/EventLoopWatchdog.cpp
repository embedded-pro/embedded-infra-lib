#include "services/util/EventLoopWatchdog.hpp"
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
    EventLoopWatchdog::EventLoopWatchdog(hal::WatchdogWithEarlyWarning& watchdog, const infra::Function<void()>& onExpired, const Config& config)
        : watchdog(watchdog)
        , expirationCount(ToNumberOfPeriods(config.expirationTimeout, watchdog.EarlyWarningPeriod()))
        , onExpired(onExpired)
    {
        really_assert(config.feedInterval > infra::Duration::zero());
        really_assert(config.feedInterval < config.expirationTimeout);

        feedTimer.Start(config.feedInterval, [this]()
            {
                Feed();
            });

        watchdog.Start([this]()
            {
                EarlyWarning();
            });
    }

    void EventLoopWatchdog::Refresh()
    {
        watchdog.Refresh();
    }

    void EventLoopWatchdog::Feed()
    {
        missedFeeds = 0;
    }

    void EventLoopWatchdog::EarlyWarning()
    {
        // Once expired the watchdog is no longer refreshed, so the hardware resets the device if onExpired returns
        if (expired)
            return;

        watchdog.Refresh();

        if (++missedFeeds == expirationCount)
        {
            expired = true;
            onExpired();
        }
    }
}
