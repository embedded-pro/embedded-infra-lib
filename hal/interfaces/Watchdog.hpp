#ifndef HAL_WATCHDOG_HPP
#define HAL_WATCHDOG_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"

namespace hal
{
    class Watchdog
    {
    protected:
        Watchdog() = default;
        Watchdog(const Watchdog& other) = delete;
        Watchdog& operator=(const Watchdog& other) = delete;
        ~Watchdog() = default;

    public:
        virtual void Refresh() = 0;
    };

    class WatchdogWithEarlyWarning
        : public Watchdog
    {
    protected:
        WatchdogWithEarlyWarning() = default;
        WatchdogWithEarlyWarning(const WatchdogWithEarlyWarning& other) = delete;
        WatchdogWithEarlyWarning& operator=(const WatchdogWithEarlyWarning& other) = delete;
        ~WatchdogWithEarlyWarning() = default;

    public:
        virtual infra::Duration EarlyWarningPeriod() const = 0;

        // Starts the watchdog, which cannot be stopped portably. onEarlyWarning is invoked from interrupt context
        // once per EarlyWarningPeriod, and the watchdog resets the device unless onEarlyWarning calls Refresh
        virtual void Start(const infra::Function<void()>& onEarlyWarning) = 0;
    };
}

#endif
