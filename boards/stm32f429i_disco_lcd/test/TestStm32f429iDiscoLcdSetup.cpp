#include "boards/stm32f429i_disco_lcd/Stm32f429iDiscoLcdSetup.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    struct ExpectedCommand
    {
        uint8_t command;
        std::vector<uint8_t> parameters;
        uint16_t delayAfterInMilliseconds;
    };

    const std::vector<ExpectedCommand> expectedCommands{
        { 0xca, { 0xc3, 0x08, 0x50 }, 0 },
        { 0xcf, { 0x00, 0xc1, 0x30 }, 0 },
        { 0xed, { 0x64, 0x03, 0x12, 0x81 }, 0 },
        { 0xe8, { 0x85, 0x00, 0x78 }, 0 },
        { 0xcb, { 0x39, 0x2c, 0x00, 0x34, 0x02 }, 0 },
        { 0xf7, { 0x20 }, 0 },
        { 0xea, { 0x00, 0x00 }, 0 },
        { 0xb1, { 0x00, 0x1b }, 0 },
        { 0xb6, { 0x0a, 0xa2 }, 0 },
        { 0xc0, { 0x10 }, 0 },
        { 0xc1, { 0x10 }, 0 },
        { 0xc5, { 0x45, 0x15 }, 0 },
        { 0xc7, { 0x90 }, 0 },
        { 0x36, { 0xc8 }, 0 },
        { 0xf2, { 0x00 }, 0 },
        { 0xb0, { 0xc2 }, 0 },
        { 0xb6, { 0x0a, 0xa7, 0x27, 0x04 }, 0 },
        { 0x2a, { 0x00, 0x00, 0x00, 0xef }, 0 },
        { 0x2b, { 0x00, 0x00, 0x01, 0x3f }, 0 },
        { 0xf6, { 0x01, 0x00, 0x06 }, 0 },
        { 0x2c, {}, 200 },
        { 0x26, { 0x01 }, 0 },
        { 0xe0, { 0x0f, 0x29, 0x24, 0x0c, 0x0e, 0x09, 0x4e, 0x78, 0x3c, 0x09, 0x13, 0x05, 0x17, 0x11, 0x00 }, 0 },
        { 0xe1, { 0x00, 0x16, 0x1b, 0x04, 0x11, 0x07, 0x31, 0x33, 0x42, 0x05, 0x0c, 0x0a, 0x28, 0x2f, 0x0f }, 0 },
        { 0x11, {}, 200 },
        { 0x29, {}, 0 },
        { 0x2c, {}, 0 },
    };

    class Stm32f429iDiscoLcdSetupTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectCommand(const ExpectedCommand& expected)
        {
            hal::SpiAction commandAction = expected.parameters.empty() ? hal::SpiAction::stop : hal::SpiAction::continueSession;

            EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ expected.command }, commandAction)).WillOnce(testing::Invoke([this](std::vector<uint8_t>, hal::SpiAction)
                {
                    EXPECT_FALSE(dataCommand.GetStubState());
                }));

            if (!expected.parameters.empty())
                EXPECT_CALL(spi, SendDataMock(expected.parameters, hal::SpiAction::stop)).WillOnce(testing::Invoke([this](std::vector<uint8_t>, hal::SpiAction)
                    {
                        EXPECT_TRUE(dataCommand.GetStubState());
                    }));
        }

        void Create()
        {
            EXPECT_CALL(spi, SetChipSelectConfigurator(testing::_)).WillOnce(testing::Invoke([this](hal::ChipSelectConfigurator& configurator)
                {
                    chipSelectConfigurator = &configurator;
                }));

            board.emplace(spi, chipSelect, dataCommand, [this]()
                {
                    initialized.callback();
                });
        }

        testing::StrictMock<hal::SpiMock> spi;
        hal::GpioPinStub chipSelect;
        hal::GpioPinStub dataCommand;
        testing::StrictMock<infra::MockCallback<void()>> initialized;
        hal::ChipSelectConfigurator* chipSelectConfigurator{ nullptr };
        std::optional<boards::Stm32f429iDiscoLcdSetup> board;
    };
}

TEST(Stm32f429iDiscoLcdPanelTest, the_set_up_has_the_commands_parameters_and_delays_of_the_board_support_package)
{
    ASSERT_EQ(expectedCommands.size(), boards::stm32f429iDiscoLcdPanel.commands.size());

    for (std::size_t i = 0; i != expectedCommands.size(); ++i)
    {
        const auto& command = boards::stm32f429iDiscoLcdPanel.commands[i];

        EXPECT_EQ(expectedCommands[i].command, command.command) << i;
        EXPECT_EQ(expectedCommands[i].parameters, std::vector<uint8_t>(command.parameters.begin(), command.parameters.end())) << i;
        EXPECT_EQ(expectedCommands[i].delayAfterInMilliseconds, command.delayAfterInMilliseconds) << i;
    }
}

TEST_F(Stm32f429iDiscoLcdSetupTest, the_commands_are_sent_over_spi_with_the_data_command_line_following_the_byte_kind)
{
    {
        testing::InSequence sequence;

        for (const auto& command : expectedCommands)
            ExpectCommand(command);

        EXPECT_CALL(initialized, callback());
    }

    Create();
    ForwardTime(std::chrono::milliseconds(400));
}

TEST_F(Stm32f429iDiscoLcdSetupTest, initialization_is_reported_after_the_last_delay_only)
{
    EXPECT_CALL(spi, SendDataMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    Create();

    ForwardTime(std::chrono::milliseconds(399));
    testing::Mock::VerifyAndClearExpectations(&initialized);

    EXPECT_CALL(initialized, callback());
    ForwardTime(std::chrono::milliseconds(1));
}

TEST_F(Stm32f429iDiscoLcdSetupTest, the_chip_select_pin_follows_the_sessions_of_the_spi_master)
{
    EXPECT_CALL(spi, SendDataMock(testing::_, testing::_)).Times(testing::AtLeast(0));
    EXPECT_CALL(initialized, callback()).Times(testing::AtLeast(0));
    Create();
    ASSERT_NE(nullptr, chipSelectConfigurator);

    EXPECT_TRUE(chipSelect.GetStubState());
    chipSelectConfigurator->StartSession();
    EXPECT_FALSE(chipSelect.GetStubState());
    chipSelectConfigurator->EndSession();
    EXPECT_TRUE(chipSelect.GetStubState());
}

TEST_F(Stm32f429iDiscoLcdSetupTest, the_data_command_line_idles_at_the_data_level_after_initialization)
{
    EXPECT_CALL(spi, SendDataMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    EXPECT_CALL(initialized, callback());
    Create();

    ForwardTime(std::chrono::milliseconds(400));

    EXPECT_TRUE(dataCommand.GetStubState());
}
