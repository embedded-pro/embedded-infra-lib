#include "hal/cortex_m/LowPowerStrategyWithModes.hpp"
#include "hal/cortex_m/SystemTickTimerService.hpp"
#include "hal/interfaces/test_doubles/LowPowerModeMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/timer/Timer.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

class LowPowerStrategyWithModesTest
    : public testing::Test
    , public infra::EventDispatcherFixture
{
public:
    LowPowerStrategyWithModesTest()
        : timerService(25000000u)
    {}

    hal::cortex::InterruptTable::WithStorage<64> interruptTable;
    hal::cortex::SystemTickTimerService timerService;
    testing::StrictMock<hal::LowPowerModeMock> lowPowerMode;
    infra::MainClockReference mainClock;
    hal::cortex::LowPowerStrategyWithModes strategy{ lowPowerMode, mainClock };
};

TEST_F(LowPowerStrategyWithModesTest, idle_without_work_timers_or_clock_references_enters_deep_sleep)
{
    EXPECT_CALL(lowPowerMode, Enter(hal::PowerMode::deepSleep));
    strategy.Idle(*this);
}

TEST_F(LowPowerStrategyWithModesTest, a_referenced_main_clock_limits_idle_to_sleep)
{
    mainClock.Refere();

    EXPECT_CALL(lowPowerMode, Enter(hal::PowerMode::sleep));
    strategy.Idle(*this);
}

TEST_F(LowPowerStrategyWithModesTest, a_released_main_clock_allows_deep_sleep_again)
{
    mainClock.Refere();
    mainClock.Release();

    EXPECT_CALL(lowPowerMode, Enter(hal::PowerMode::deepSleep));
    strategy.Idle(*this);
}

TEST_F(LowPowerStrategyWithModesTest, a_pending_timer_limits_idle_to_sleep)
{
    infra::TimerSingleShot timer(std::chrono::seconds(1), []() {});

    EXPECT_CALL(lowPowerMode, Enter(hal::PowerMode::sleep));
    strategy.Idle(*this);
}

TEST_F(LowPowerStrategyWithModesTest, a_cancelled_timer_allows_deep_sleep_again)
{
    infra::TimerSingleShot timer(std::chrono::seconds(1), []() {});
    timer.Cancel();

    EXPECT_CALL(lowPowerMode, Enter(hal::PowerMode::deepSleep));
    strategy.Idle(*this);
}

TEST_F(LowPowerStrategyWithModesTest, idle_with_scheduled_work_does_not_enter_low_power)
{
    Schedule([]() {});

    strategy.Idle(*this);

    ExecuteAllActions();
}

TEST_F(LowPowerStrategyWithModesTest, low_power_is_entered_with_interrupts_masked)
{
    uint32_t primask = 0;
    EXPECT_CALL(lowPowerMode, Enter(testing::_)).WillOnce([&primask](hal::PowerMode)
        {
            __asm volatile("mrs %0, primask" : "=r"(primask));
        });

    strategy.Idle(*this);

    EXPECT_EQ(1u, primask);
}

TEST_F(LowPowerStrategyWithModesTest, idle_restores_the_interrupt_mask)
{
    EXPECT_CALL(lowPowerMode, Enter(testing::_));
    strategy.Idle(*this);

    uint32_t primask = 1;
    __asm volatile("mrs %0, primask" : "=r"(primask));
    EXPECT_EQ(0u, primask);
}
