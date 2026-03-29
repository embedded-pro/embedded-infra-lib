#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/peripheral/GpioPinInverted.hpp"
#include "gtest/gtest.h"

class InverseLogicPinTest
    : public testing::Test
    , public infra::ClockFixture
{
public:
    InverseLogicPinTest()
        : gpioPin(basePin)
    {}

    infra::MockCallback<void()> callback;
    hal::GpioPinSpy basePin;
    services::GpioPinInverted gpioPin;
};

TEST_F(InverseLogicPinTest, GpioPinGetValue)
{
    hal::InputPin pin(gpioPin);

    basePin.SetStubState(true);
    EXPECT_FALSE(pin.Get());
    basePin.SetStubState(false);
    EXPECT_TRUE(pin.Get());
}

TEST_F(InverseLogicPinTest, GpioPinTriggerOnChange)
{
    hal::InputPin pin(gpioPin);

    infra::MockCallback<void()> callback;
    EXPECT_CALL(callback, callback());

    pin.EnableInterrupt([&callback]()
        {
            callback.callback();
        },
        hal::InterruptTrigger::fallingEdge);
    basePin.SetStubState(true);
}

TEST_F(InverseLogicPinTest, GpioPinSetValue)
{
    hal::OutputPin pin(gpioPin);

    pin.Set(true);
    EXPECT_FALSE(basePin.GetStubState());
    pin.Set(false);
    EXPECT_TRUE(basePin.GetStubState());
}

TEST_F(InverseLogicPinTest, GpioPinGetOutputValue)
{
    hal::OutputPin pin(gpioPin);

    pin.Set(true);
    EXPECT_TRUE(pin.GetOutputLatch());
    pin.Set(false);
    EXPECT_FALSE(pin.GetOutputLatch());
}

TEST_F(InverseLogicPinTest, SetAsInput)
{
    gpioPin.SetAsInput();
    EXPECT_TRUE(basePin.IsInput());
}

TEST_F(InverseLogicPinTest, IsInput)
{
    gpioPin.SetAsInput();
    EXPECT_TRUE(gpioPin.IsInput());

    hal::OutputPin pin(gpioPin);
    pin.Set(true);
    EXPECT_FALSE(gpioPin.IsInput());
}

TEST_F(InverseLogicPinTest, ConfigWithoutStartState)
{
    gpioPin.Config(hal::PinConfigType::input);
    EXPECT_TRUE(basePin.IsInput());
}

TEST_F(InverseLogicPinTest, ConfigWithStartState)
{
    gpioPin.Config(hal::PinConfigType::output, true);
    EXPECT_TRUE(basePin.GetStubState());

    gpioPin.Config(hal::PinConfigType::output, false);
    EXPECT_FALSE(basePin.GetStubState());
}

TEST_F(InverseLogicPinTest, ResetConfig)
{
    gpioPin.ResetConfig();
}

TEST_F(InverseLogicPinTest, DisableInterrupt)
{
    hal::InputPin pin(gpioPin);
    pin.EnableInterrupt([]() {}, hal::InterruptTrigger::risingEdge);
    gpioPin.DisableInterrupt();
}

TEST_F(InverseLogicPinTest, EnableInterruptWithRisingEdge)
{
    infra::MockCallback<void()> callback;
    EXPECT_CALL(callback, callback());

    gpioPin.EnableInterrupt([&callback]()
        {
            callback.callback();
        },
        hal::InterruptTrigger::risingEdge, hal::InterruptType::dispatched);
    basePin.SetStubState(false);
}

TEST_F(InverseLogicPinTest, EnableInterruptWithBothEdges)
{
    infra::MockCallback<void()> callback;
    EXPECT_CALL(callback, callback());

    gpioPin.EnableInterrupt([&callback]()
        {
            callback.callback();
        },
        hal::InterruptTrigger::bothEdges, hal::InterruptType::dispatched);
    basePin.SetStubState(true);
}
