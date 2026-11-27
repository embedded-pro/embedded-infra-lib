#include "hal/generic/TimerServiceGeneric.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/timer/Timer.hpp"
#include "gtest/gtest.h"
#include <chrono>

namespace
{
    constexpr uint32_t timerServiceId = infra::systemTimerServiceId + 1;

    class TimerServiceGenericTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {};
}

// The shutdown race is not deterministic: many cycles make a lost wake-up likely, and a deadlock shows as a test timeout
TEST_F(TimerServiceGenericTest, destruction_after_a_timer_was_armed_finishes)
{
    for (int cycle = 0; cycle != 2000; ++cycle)
    {
        hal::TimerServiceGeneric timerService{ timerServiceId };
        infra::TimerSingleShot timer{ std::chrono::hours(1), [] {}, timerServiceId };
    }
}

TEST_F(TimerServiceGenericTest, destruction_without_timer_finishes)
{
    for (int cycle = 0; cycle != 2000; ++cycle)
        hal::TimerServiceGeneric timerService{ timerServiceId };
}
