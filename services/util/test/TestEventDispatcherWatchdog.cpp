#include "hal/interfaces/test_doubles/WatchdogMock.hpp"
#include "infra/util/SharedObjectAllocatorFixedSize.hpp"
#include "services/util/EventDispatcherWatchdog.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <optional>

namespace
{
    class LowPowerStrategyStub
        : public infra::LowPowerStrategy
    {
    public:
        void RequestExecution() override
        {}

        void Idle(const infra::EventDispatcherWorker& eventDispatcher) override
        {}
    };
}

template<class Dispatcher>
class EventDispatcherWatchdogTest
    : public testing::Test
{
public:
    EventDispatcherWatchdogTest()
    {
        EXPECT_CALL(watchdog, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
        EXPECT_CALL(watchdog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
        eventDispatcher.emplace(watchdog, std::chrono::milliseconds(150), onExpired);
    }

    void EarlyWarning()
    {
        EXPECT_CALL(watchdog, Refresh());
        earlyWarning();
    }

    void WhileAnActionRuns(const infra::Function<void()>& duringAction)
    {
        eventDispatcher->Schedule(duringAction);
        eventDispatcher->ExecuteAllActions();
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

    testing::StrictMock<hal::WatchdogMock> watchdog;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> onExpired{ [this]()
        {
            expired.Call();
        } };
    infra::Function<void()> earlyWarning;
    std::optional<typename Dispatcher::template WithSize<10>> eventDispatcher;
};

using Dispatchers = testing::Types<services::EventDispatcherWithWatchdog, services::EventDispatcherWithWeakPtrAndWatchdog>;
TYPED_TEST_SUITE(EventDispatcherWatchdogTest, Dispatchers);

TYPED_TEST(EventDispatcherWatchdogTest, construction_starts_the_watchdog)
{
    EXPECT_TRUE(static_cast<bool>(this->earlyWarning));
}

TYPED_TEST(EventDispatcherWatchdogTest, an_early_warning_refreshes_the_watchdog)
{
    this->EarlyWarning();
}

TYPED_TEST(EventDispatcherWatchdogTest, an_idle_event_dispatcher_does_not_expire)
{
    for (uint32_t iteration = 0; iteration != 10; ++iteration)
        this->EarlyWarning();
}

TYPED_TEST(EventDispatcherWatchdogTest, an_event_dispatcher_that_keeps_executing_actions_does_not_expire)
{
    for (uint32_t iteration = 0; iteration != 10; ++iteration)
        this->WhileAnActionRuns([this]()
            {
                this->EarlyWarning();
            });
}

TYPED_TEST(EventDispatcherWatchdogTest, an_action_that_does_not_return_expires_the_watchdog)
{
    this->ExpireInAStuckAction();
}

TYPED_TEST(EventDispatcherWatchdogTest, an_action_shorter_than_the_expiration_timeout_does_not_expire)
{
    this->WhileAnActionRuns([this]()
        {
            this->EarlyWarning();
            this->EarlyWarning();
            this->EarlyWarning();
        });

    this->EarlyWarning();
}

TYPED_TEST(EventDispatcherWatchdogTest, an_expired_watchdog_is_no_longer_refreshed)
{
    this->ExpireInAStuckAction();

    this->earlyWarning();
    this->earlyWarning();
}

class EventDispatcherWatchdogVariantsTest
    : public testing::Test
{
public:
    testing::StrictMock<hal::WatchdogMock> watchdog;
    testing::StrictMock<testing::MockFunction<void()>> expired;
    infra::Function<void()> onExpired{ [this]()
        {
            expired.Call();
        } };
    infra::Function<void()> earlyWarning;
};

TEST_F(EventDispatcherWatchdogVariantsTest, the_expiration_timeout_is_rounded_up_to_whole_early_warning_periods)
{
    EXPECT_CALL(watchdog, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
    EXPECT_CALL(watchdog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
    services::EventDispatcherWithWatchdog::WithSize<10> eventDispatcher(watchdog, std::chrono::milliseconds(120), onExpired);

    eventDispatcher.Schedule([this]()
        {
            EXPECT_CALL(watchdog, Refresh()).Times(4);
            earlyWarning();
            earlyWarning();
            earlyWarning();

            EXPECT_CALL(expired, Call());
            earlyWarning();
        });
    eventDispatcher.ExecuteAllActions();
}

TEST_F(EventDispatcherWatchdogVariantsTest, extends_the_low_power_event_dispatcher)
{
    LowPowerStrategyStub lowPowerStrategy;
    EXPECT_CALL(watchdog, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
    EXPECT_CALL(watchdog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
    services::LowPowerEventDispatcherWithWeakPtrAndWatchdog::WithSize<10> eventDispatcher(watchdog, std::chrono::milliseconds(50), onExpired, lowPowerStrategy);

    eventDispatcher.Schedule([this]()
        {
            EXPECT_CALL(watchdog, Refresh()).Times(2);
            earlyWarning();

            EXPECT_CALL(expired, Call());
            earlyWarning();
        });
    eventDispatcher.ExecuteAllActions();
}

TEST_F(EventDispatcherWatchdogVariantsTest, the_low_power_event_dispatcher_supervises_actions_scheduled_with_a_weak_ptr)
{
    LowPowerStrategyStub lowPowerStrategy;
    EXPECT_CALL(watchdog, EarlyWarningPeriod()).WillRepeatedly(testing::Return(std::chrono::milliseconds(50)));
    EXPECT_CALL(watchdog, Start(testing::_)).WillOnce(testing::SaveArg<0>(&earlyWarning));
    services::LowPowerEventDispatcherWithWeakPtrAndWatchdog::WithSize<10> eventDispatcher(watchdog, std::chrono::milliseconds(50), onExpired, lowPowerStrategy);

    infra::SharedObjectAllocatorFixedSize<int, void()>::WithStorage<1> allocator;
    infra::SharedPtr<int> object = allocator.Allocate();
    infra::WeakPtr<int> weakObject = object;

    eventDispatcher.Schedule([this](const infra::SharedPtr<int>& object)
        {
            EXPECT_CALL(watchdog, Refresh()).Times(2);
            earlyWarning();

            EXPECT_CALL(expired, Call());
            earlyWarning();
        },
        weakObject);
    eventDispatcher.ExecuteAllActions();
}
