#include "hal/interfaces/test_doubles/WatchdogMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/util/EventLoopWatchdog.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <optional>

class EventLoopWatchdogTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    EventLoopWatchdogTest()
    {
        config.feedInterval = std::chrono::milliseconds(25);
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

    testing::StrictMock<hal::WatchdogWithEarlyWarningMock> hardware;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> onExpired{ [this]()
        {
            expired.Call();
        } };
    services::EventLoopWatchdog::Config config;
    infra::Function<void()> earlyWarning;
    std::optional<services::EventLoopWatchdog> watchdog;
};

TEST_F(EventLoopWatchdogTest, construction_starts_the_hardware_watchdog)
{
    EXPECT_TRUE(static_cast<bool>(earlyWarning));
}

TEST_F(EventLoopWatchdogTest, an_early_warning_refreshes_the_hardware_watchdog)
{
    EarlyWarning();
}

TEST_F(EventLoopWatchdogTest, a_fed_watchdog_does_not_expire)
{
    for (uint32_t iteration = 0; iteration != 10; ++iteration)
    {
        EarlyWarning();
        ForwardTime(std::chrono::milliseconds(25));
    }
}

TEST_F(EventLoopWatchdogTest, consecutive_early_warnings_without_a_feed_expire_the_watchdog)
{
    EarlyWarning();
    EarlyWarning();

    EXPECT_CALL(expired, Call());
    EarlyWarning();
}

TEST_F(EventLoopWatchdogTest, a_feed_restarts_the_count_towards_expiry)
{
    EarlyWarning();
    EarlyWarning();

    ForwardTime(std::chrono::milliseconds(25));

    EarlyWarning();
    EarlyWarning();

    EXPECT_CALL(expired, Call());
    EarlyWarning();
}

TEST_F(EventLoopWatchdogTest, an_expired_watchdog_is_no_longer_refreshed)
{
    EarlyWarning();
    EarlyWarning();
    EXPECT_CALL(expired, Call());
    EarlyWarning();

    EarlyWarningWithoutRefresh();
    EarlyWarningWithoutRefresh();
}

TEST_F(EventLoopWatchdogTest, a_feed_after_expiry_does_not_resume_refreshing)
{
    EarlyWarning();
    EarlyWarning();
    EXPECT_CALL(expired, Call());
    EarlyWarning();

    ForwardTime(std::chrono::milliseconds(25));

    EarlyWarningWithoutRefresh();
}

TEST_F(EventLoopWatchdogTest, refresh_refreshes_the_hardware_watchdog)
{
    EXPECT_CALL(hardware, Refresh());
    watchdog->Refresh();
}

TEST(EventLoopWatchdogExpirationCountTest, the_expiration_timeout_is_rounded_up_to_whole_early_warning_periods)
{
    infra::ClockFixture clock;
    testing::StrictMock<hal::WatchdogWithEarlyWarningMock> hardware;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> earlyWarning;

    services::EventLoopWatchdog::Config config;
    config.expirationTimeout = std::chrono::milliseconds(120);

    EXPECT_CALL(hardware, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
    EXPECT_CALL(hardware, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
    services::EventLoopWatchdog watchdog(hardware, [&expired]()
        {
            expired.Call();
        },
        config);

    EXPECT_CALL(hardware, Refresh()).Times(3);
    earlyWarning();
    earlyWarning();

    EXPECT_CALL(expired, Call());
    earlyWarning();
}
