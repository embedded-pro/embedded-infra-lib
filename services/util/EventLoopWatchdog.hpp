#ifndef SERVICES_EVENT_LOOP_WATCHDOG_HPP
#define SERVICES_EVENT_LOOP_WATCHDOG_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>

namespace services
{
    class EventLoopWatchdog
        : public hal::Watchdog
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration feedInterval{ std::chrono::milliseconds(25) };
            infra::Duration expirationTimeout{ std::chrono::milliseconds(1500) };
        };

        EventLoopWatchdog(hal::WatchdogWithEarlyWarning& watchdog, const infra::Function<void()>& onExpired, const Config& config = Config());

        void Refresh() override;

    private:
        void Feed();
        void EarlyWarning();

        hal::WatchdogWithEarlyWarning& watchdog;
        uint32_t expirationCount;
        std::atomic<uint32_t> missedFeeds{ 0 };
        std::atomic<bool> expired{ false };
        infra::Function<void()> onExpired;
        infra::TimerRepeating feedTimer;
    };
}

#endif
