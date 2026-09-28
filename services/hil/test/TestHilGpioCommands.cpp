#include "services/hil/commands/HilGpioCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

class HilGpioCommandsTest
    : public services::HilFixture
{
public:
    void ConfigureInput()
    {
        EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::input));
        Execute("gpio.cfg PB2 in");
        ASSERT_EQ("OK\r\n", Output());
    }

    void ConfigureOutput()
    {
        EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::output, false));
        Execute("gpio.cfg PB2 out");
        ASSERT_EQ("OK\r\n", Output());
    }

    services::HilGpioCommands::WithMaxPins<2> gpio{ context };
};

TEST_F(HilGpioCommandsTest, configure_input)
{
    ConfigureInput();

    EXPECT_EQ((services::HilPinId{ 1, 2 }), pinFactory.constructed[0]);
    EXPECT_EQ(services::HilPull::none, pinFactory.options[0].pull);
    EXPECT_FALSE(pinFactory.options[0].openDrain);
}

TEST_F(HilGpioCommandsTest, configure_takes_alias_pull_and_options)
{
    EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::input));
    Execute("gpio.cfg id0 in");
    EXPECT_EQ(services::HilPull::up, pinFactory.options[0].pull);

    EXPECT_CALL(pinFactory.pins[1], Config(hal::PinConfigType::output, false));
    Execute("gpio.cfg PB3 out pull=down drive=8");
    EXPECT_EQ(services::HilPull::down, pinFactory.options[1].pull);
    EXPECT_EQ(2, pinFactory.options[1].drive);

    EXPECT_EQ("OK\r\nOK\r\n", Output());
}

TEST_F(HilGpioCommandsTest, configure_open_drain_starts_released)
{
    EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::output, true));
    Execute("gpio.cfg PB2 od");
    Execute("gpio.cfg PB3 od pull=up");

    EXPECT_EQ("OK\r\nERR usage\r\n", Output());
    EXPECT_TRUE(pinFactory.options[0].openDrain);
}

TEST_F(HilGpioCommandsTest, configure_reports_errors)
{
    Execute("gpio.cfg PB2");
    Execute("gpio.cfg PB2 sideways");
    Execute("gpio.cfg PZ2 in");
    Execute("gpio.cfg PB2 in drive=3");
    Execute("gpio.cfg PB2 in speed=1");
    Execute("gpio.cfg PA0 in");

    EXPECT_EQ("ERR usage\r\nERR usage\r\nERR pin\r\nERR usage\r\nERR usage\r\nERR busy\r\n", Output());
}

TEST_F(HilGpioCommandsTest, configure_is_busy_when_all_entries_are_used)
{
    EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::input));
    EXPECT_CALL(pinFactory.pins[1], Config(hal::PinConfigType::input));
    Execute("gpio.cfg PB0 in");
    Execute("gpio.cfg PB1 in");
    Execute("gpio.cfg PB2 in");

    EXPECT_EQ("OK\r\nOK\r\nERR busy\r\n", Output());
}

TEST_F(HilGpioCommandsTest, reconfigure_releases_the_previous_configuration)
{
    ConfigureInput();

    EXPECT_CALL(pinFactory.pins[0], ResetConfig());
    EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::output, false));
    Execute("gpio.cfg PB2 out");

    EXPECT_EQ("OK\r\n", Output());
}

TEST_F(HilGpioCommandsTest, set_and_get)
{
    ConfigureOutput();

    EXPECT_CALL(pinFactory.pins[0], Set(true));
    Execute("gpio.set PB2 1");
    EXPECT_CALL(pinFactory.pins[0], Get()).WillOnce(testing::Return(true));
    Execute("gpio.get PB2");
    Execute("gpio.set PB2 2");
    Execute("gpio.get PB3");

    EXPECT_EQ("OK\r\nOK value=1\r\nERR usage\r\nERR notopen\r\n", Output());
}

TEST_F(HilGpioCommandsTest, pulse_toggles_and_reports_when_done)
{
    ConfigureOutput();

    Execute("gpio.pulse PB2 2 10");
    Execute("gpio.pulse PB2 2 10");
    EXPECT_EQ("ERR busy\r\n", Output());

    EXPECT_CALL(pinFactory.pins[0], GetOutputLatch()).WillOnce(testing::Return(false));
    EXPECT_CALL(pinFactory.pins[0], Set(true));
    ForwardTime(std::chrono::milliseconds(10));
    EXPECT_EQ("", Output());

    EXPECT_CALL(pinFactory.pins[0], GetOutputLatch()).WillOnce(testing::Return(true));
    EXPECT_CALL(pinFactory.pins[0], Set(false));
    ForwardTime(std::chrono::milliseconds(10));
    EXPECT_EQ("\r\nOK\r\n", Output());
}

TEST_F(HilGpioCommandsTest, pulse_needs_an_output)
{
    ConfigureInput();

    Execute("gpio.pulse PB2 1 10");
    Execute("gpio.pulse PB2 0 10");

    EXPECT_EQ("ERR usage\r\nERR range\r\n", Output());
}

TEST_F(HilGpioCommandsTest, pulsing_pin_cannot_be_released_or_reconfigured)
{
    ConfigureOutput();
    Execute("gpio.pulse PB2 1 10");

    Execute("gpio.release PB2");
    Execute("gpio.cfg PB2 in");

    EXPECT_EQ("ERR busy\r\nERR busy\r\n", Output());
}

TEST_F(HilGpioCommandsTest, interrupt_counts_edges)
{
    ConfigureInput();
    infra::Function<void()> onEdge;

    EXPECT_CALL(pinFactory.pins[0], EnableInterrupt(testing::_, hal::InterruptTrigger::fallingEdge, hal::InterruptType::immediate)).WillOnce(testing::SaveArg<0>(&onEdge));
    Execute("gpio.irq PB2 falling type=immediate");
    onEdge();
    onEdge();
    Execute("gpio.count PB2 clear=1");
    Execute("gpio.count PB2");

    EXPECT_EQ("OK\r\nOK count=2\r\nOK count=0\r\n", Output());
}

TEST_F(HilGpioCommandsTest, interrupt_off_disables)
{
    ConfigureInput();

    EXPECT_CALL(pinFactory.pins[0], EnableInterrupt(testing::_, hal::InterruptTrigger::bothEdges, hal::InterruptType::dispatched));
    Execute("gpio.irq PB2 both");
    EXPECT_CALL(pinFactory.pins[0], DisableInterrupt());
    Execute("gpio.irq PB2 off");

    EXPECT_EQ("OK\r\nOK\r\n", Output());
}

TEST_F(HilGpioCommandsTest, interrupt_on_unsupported_pin)
{
    EXPECT_CALL(pinFactory.pins[0], Config(hal::PinConfigType::input));
    Execute("gpio.cfg PF2 in");
    Execute("gpio.irq PF2 rising");

    EXPECT_EQ("OK\r\nERR unsupported\r\n", Output());
}

TEST_F(HilGpioCommandsTest, release_frees_the_pin)
{
    ConfigureInput();
    EXPECT_CALL(pinFactory.pins[0], EnableInterrupt(testing::_, hal::InterruptTrigger::risingEdge, hal::InterruptType::dispatched));
    Execute("gpio.irq PB2 rising");

    EXPECT_CALL(pinFactory.pins[0], DisableInterrupt());
    EXPECT_CALL(pinFactory.pins[0], ResetConfig());
    Execute("gpio.release PB2");
    Execute("gpio.get PB2");

    EXPECT_EQ("OK\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}
