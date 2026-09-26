#ifndef SERVICES_EVENT_DISPATCHER_WATCHDOG_HPP
#define SERVICES_EVENT_DISPATCHER_WATCHDOG_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "infra/event/ExecutionProgress.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include <chrono>
#include <cstdint>

namespace services
{
    class EventDispatcherWatchdog
        : public hal::Watchdog
    {
    public:
        struct Config
        {
            constexpr Config()
            {}

            infra::Duration expirationTimeout{ std::chrono::milliseconds(1500) };
        };

        EventDispatcherWatchdog(hal::WatchdogWithEarlyWarning& watchdog, const infra::Function<void()>& onExpired, const Config& config = Config());

        void Refresh() override;

    private:
        bool EventDispatcherProgressed();
        void EarlyWarning();

        hal::WatchdogWithEarlyWarning& watchdog;
        const infra::ExecutionProgress& progress;
        uint32_t expirationCount;
        uint32_t stepsAtLastEarlyWarning;
        uint32_t missedEarlyWarnings{ 0 };
        bool expired{ false };
        infra::Function<void()> onExpired;
    };
}

#endif
