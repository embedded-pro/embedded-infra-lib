#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/timer/TickOnInterruptTimerService.hpp"
#include "infra/timer/Timer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class SystemTickTimerServiceTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    SystemTickTimerServiceTest()
        : timerService(infra::systemTimerServiceId, std::chrono::milliseconds(1))
    {}

    infra::TickOnInterruptTimerService timerService;
};

TEST_F(SystemTickTimerServiceTest, timer_fires_after_two_tick_interrupts)
{
    bool fired = false;
    infra::TimerSingleShot timer(std::chrono::milliseconds(1), [&fired]()
        {
            fired = true;
        });

    timerService.SystemTickInterrupt();
    ExecuteAllActions();
    timerService.SystemTickInterrupt();
    ExecuteAllActions();

    EXPECT_TRUE(fired);
}

TEST_F(SystemTickTimerServiceTest, time_advances_per_tick)
{
    ASSERT_EQ(infra::TimePoint(), timerService.Now());
    timerService.SystemTickInterrupt();
    ExecuteAllActions();
    EXPECT_EQ(infra::TimePoint() + std::chrono::milliseconds(1), timerService.Now());
}

TEST_F(SystemTickTimerServiceTest, multiple_ticks_advance_time)
{
    for (int i = 0; i < 5; ++i)
    {
        timerService.SystemTickInterrupt();
        ExecuteAllActions();
    }
    EXPECT_EQ(infra::TimePoint() + std::chrono::milliseconds(5), timerService.Now());
}
