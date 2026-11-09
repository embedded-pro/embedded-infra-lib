#include "hal/null/GpioPinNull.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(GpioPinNullTest, reads_low_after_configuring_and_setting_high)
{
    hal::GpioPinNull pin;

    pin.Config(hal::PinConfigType::output);
    pin.Config(hal::PinConfigType::output, true);
    pin.Set(true);

    EXPECT_FALSE(pin.Get());
    EXPECT_FALSE(pin.GetOutputLatch());
}

TEST(GpioPinNullTest, IsInput_stays_false_after_SetAsInput)
{
    hal::GpioPinNull pin;

    pin.SetAsInput();

    EXPECT_FALSE(pin.IsInput());
}

TEST(GpioPinNullTest, interrupt_never_fires)
{
    hal::GpioPinNull pin;
    testing::StrictMock<infra::MockCallback<void()>> onInterrupt;

    pin.EnableInterrupt([&onInterrupt]
        {
            onInterrupt.callback();
        },
        hal::InterruptTrigger::bothEdges, hal::InterruptType::dispatched);
    pin.Set(true);
    pin.Set(false);
    pin.DisableInterrupt();
    pin.ResetConfig();
}
