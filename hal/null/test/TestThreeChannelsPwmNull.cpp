#include "hal/null/ThreeChannelsPwmNull.hpp"
#include "gtest/gtest.h"

TEST(ThreeChannelsPwmNullTest, accepts_a_full_start_stop_cycle)
{
    hal::ThreeChannelsPwmNull pwm;

    pwm.SetBaseFrequency(hal::Hertz{ 20000 });
    pwm.Start(hal::DutyCycle::FromPercent(25), hal::DutyCycle::FromPercent(50), hal::DutyCycle::FromPercent(75));
    pwm.Stop();

    SUCCEED();
}
