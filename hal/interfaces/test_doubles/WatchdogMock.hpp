#ifndef HAL_WATCHDOG_MOCK_HPP
#define HAL_WATCHDOG_MOCK_HPP

#include "hal/interfaces/Watchdog.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class WatchdogMock
        : public Watchdog
    {
    public:
        MOCK_METHOD(infra::Duration, EarlyWarningPeriod, (), (const, override));
        MOCK_METHOD(void, Start, (const infra::Function<void()>& onEarlyWarning), (override));
        MOCK_METHOD(void, Refresh, (), (override));
    };
}

#endif
