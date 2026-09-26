#include "hal/interfaces/test_doubles/WatchdogMock.hpp"
#include "infra/event/test_helper/EventDispatcherWithWeakPtrFixture.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

class EventDispatcherWatchdogTest
    : public testing::Test
    , public infra::EventDispatcherWithWeakPtrFixture
{
public:
    EventDispatcherWatchdogTest()
    {
        config.expirationTimeout = std::chrono::milliseconds(150);

        EXPECT_CALL(hardware, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
        EXPECT_CALL(hardware, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
        watchdog.emplace(hardware, onExpired, config);
    }

    void EarlyWarning()
    {
        EXPECT_CALL(hardware, Refresh());
        earlyWarning();
    }

    void EarlyWarningWithoutRefresh()
    {
        earlyWarning();
    }

    void WhileAnActionRuns(const infra::Function<void()>& duringAction)
    {
        infra::EventDispatcher::Instance().Schedule(duringAction);
        ExecuteAllActions();
    }

    void ExpireInAStuckAction()
    {
        WhileAnActionRuns([this]()
            {
                EarlyWarning();
                EarlyWarning();
                EarlyWarning();

                EXPECT_CALL(expired, Call());
                EarlyWarning();
            });
    }

    testing::StrictMock<hal::WatchdogWithEarlyWarningMock> hardware;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> onExpired{ [this]()
        {
            expired.Call();
        } };
    services::EventDispatcherWatchdog::Config config;
    infra::Function<void()> earlyWarning;
    std::optional<services::EventDispatcherWatchdog> watchdog;
};

TEST_F(EventDispatcherWatchdogTest, construction_starts_the_hardware_watchdog)
{
    EXPECT_TRUE(static_cast<bool>(earlyWarning));
}

TEST_F(EventDispatcherWatchdogTest, an_early_warning_refreshes_the_hardware_watchdog)
{
    EarlyWarning();
}

TEST_F(EventDispatcherWatchdogTest, an_idle_event_dispatcher_does_not_expire)
{
    for (uint32_t iteration = 0; iteration != 10; ++iteration)
        EarlyWarning();
}

TEST_F(EventDispatcherWatchdogTest, an_event_dispatcher_that_keeps_executing_actions_does_not_expire)
{
    for (uint32_t iteration = 0; iteration != 10; ++iteration)
        WhileAnActionRuns([this]()
            {
                EarlyWarning();
            });
}

TEST_F(EventDispatcherWatchdogTest, an_action_that_does_not_return_expires_the_watchdog)
{
    ExpireInAStuckAction();
}

TEST_F(EventDispatcherWatchdogTest, a_long_action_shorter_than_the_expiration_timeout_does_not_expire)
{
    WhileAnActionRuns([this]()
        {
            EarlyWarning();
            EarlyWarning();
            EarlyWarning();
        });

    EarlyWarning();
}

TEST_F(EventDispatcherWatchdogTest, an_expired_watchdog_is_no_longer_refreshed)
{
    ExpireInAStuckAction();

    EarlyWarningWithoutRefresh();
    EarlyWarningWithoutRefresh();
}

TEST_F(EventDispatcherWatchdogTest, refresh_refreshes_the_hardware_watchdog)
{
    EXPECT_CALL(hardware, Refresh());
    watchdog->Refresh();
}

class EventDispatcherWatchdogExpirationCountTest
    : public testing::Test
    , public infra::EventDispatcherWithWeakPtrFixture
{
public:
    testing::StrictMock<hal::WatchdogWithEarlyWarningMock> hardware;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> onExpired{ [this]()
        {
            expired.Call();
        } };
    infra::Function<void()> earlyWarning;
};

TEST_F(EventDispatcherWatchdogExpirationCountTest, the_expiration_timeout_is_rounded_up_to_whole_early_warning_periods)
{
    services::EventDispatcherWatchdog::Config config;
    config.expirationTimeout = std::chrono::milliseconds(120);

    EXPECT_CALL(hardware, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
    EXPECT_CALL(hardware, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
    services::EventDispatcherWatchdog watchdog(hardware, onExpired, config);

    infra::EventDispatcher::Instance().Schedule([this]()
        {
            EXPECT_CALL(hardware, Refresh()).Times(4);
            earlyWarning();
            earlyWarning();
            earlyWarning();

            EXPECT_CALL(expired, Call());
            earlyWarning();
        });
    ExecuteAllActions();
}
