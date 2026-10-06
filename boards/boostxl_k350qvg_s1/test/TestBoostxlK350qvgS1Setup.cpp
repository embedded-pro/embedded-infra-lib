#include "boards/boostxl_k350qvg_s1/BoostxlK350qvgS1Setup.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    struct Register
    {
        uint8_t index;
        uint16_t value;
    };

    class BoostxlK350qvgS1SetupTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectRegister(const Register& reg)
        {
            EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ reg.index }, hal::SpiAction::continueSession)).WillOnce(testing::Invoke([this](std::vector<uint8_t>, hal::SpiAction)
                {
                    EXPECT_FALSE(dataCommand.GetStubState());
                }));
            EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ static_cast<uint8_t>(reg.value >> 8), static_cast<uint8_t>(reg.value) }, hal::SpiAction::stop)).WillOnce(testing::Invoke([this](std::vector<uint8_t>, hal::SpiAction)
                {
                    EXPECT_TRUE(dataCommand.GetStubState());
                }));
        }

        void ExpectBringUpBeforeEntryMode()
        {
            ExpectRegister({ 0x10, 0x0001 });
            ExpectRegister({ 0x1e, 0x00ba });
            ExpectRegister({ 0x28, 0x0006 });
            ExpectRegister({ 0x00, 0x0001 });
            ExpectRegister({ 0x01, 0x30ef });
            ExpectRegister({ 0x02, 0x0600 });
            ExpectRegister({ 0x10, 0x0000 });
        }

        void ExpectBringUpAfterEntryMode()
        {
            ExpectRegister({ 0x07, 0x0033 });
            ExpectRegister({ 0x0c, 0x0005 });
            ExpectRegister({ 0x30, 0x0000 });
            ExpectRegister({ 0x31, 0x0400 });
            ExpectRegister({ 0x32, 0x0106 });
            ExpectRegister({ 0x33, 0x0700 });
            ExpectRegister({ 0x34, 0x0002 });
            ExpectRegister({ 0x35, 0x0702 });
            ExpectRegister({ 0x36, 0x0707 });
            ExpectRegister({ 0x37, 0x0203 });
            ExpectRegister({ 0x3a, 0x1400 });
            ExpectRegister({ 0x3b, 0x0f03 });
            ExpectRegister({ 0x0d, 0x0007 });
            ExpectRegister({ 0x0e, 0x3100 });
        }

        void CreateAndInitialize()
        {
            {
                testing::InSequence sequence;
                ExpectBringUpBeforeEntryMode();
                ExpectRegister({ 0x11, 0x6800 });
                ExpectBringUpAfterEntryMode();
            }
            EXPECT_CALL(initialized, callback());

            board.emplace(spi, dataCommand, reset, [this]()
                {
                    initialized.callback();
                });
            ForwardTime(std::chrono::milliseconds(60));
        }

        testing::StrictMock<hal::SpiMock> spi;
        hal::GpioPinStub dataCommand;
        hal::GpioPinStub reset;
        testing::StrictMock<infra::MockCallback<void()>> initialized;
        std::optional<boards::BoostxlK350qvgS1Setup> board;
    };
}

TEST(BoostxlK350qvgS1PanelTest, the_panel_is_320_by_240)
{
    EXPECT_EQ((hal::DisplaySize{ 320, 240 }), boards::boostxlK350qvgS1Panel.size);
}

TEST(BoostxlK350qvgS1PanelTest, the_default_landscape_orientation_starts_in_the_last_gddram_corner)
{
    EXPECT_TRUE(boards::boostxlK350qvgS1Panel.mirrorX);
    EXPECT_TRUE(boards::boostxlK350qvgS1Panel.mirrorY);
}

TEST(BoostxlK350qvgS1PanelTest, the_bring_up_waits_30_ms_after_leaving_sleep_mode_and_nowhere_else)
{
    const auto& before = boards::boostxlK350qvgS1Panel.beforeEntryMode;
    const auto& after = boards::boostxlK350qvgS1Panel.afterEntryMode;

    for (const auto& step : before)
    {
        EXPECT_EQ(&step == &before.back() ? 30 : 0, step.delayAfterInMilliseconds);
    }

    for (const auto& step : after)
    {
        EXPECT_EQ(0, step.delayAfterInMilliseconds);
    }
}

TEST_F(BoostxlK350qvgS1SetupTest, bring_up_writes_the_panel_registers_over_spi_with_the_data_command_line_following_the_phase)
{
    CreateAndInitialize();
}

TEST_F(BoostxlK350qvgS1SetupTest, the_panel_is_reported_as_a_320_by_240_display_with_wire_ordered_rgb565)
{
    CreateAndInitialize();

    EXPECT_EQ((hal::DisplaySize{ 320, 240 }), board->Display().Size());
    EXPECT_EQ(hal::PixelFormat::rgb565Swapped, board->Display().Format());
}

TEST_F(BoostxlK350qvgS1SetupTest, the_first_pixel_goes_to_the_last_gddram_position)
{
    CreateAndInitialize();
    {
        testing::InSequence sequence;
        ExpectRegister({ 0x45, 0x013f });
        ExpectRegister({ 0x46, 0x013f });
        ExpectRegister({ 0x44, 0xefef });
        ExpectRegister({ 0x4e, 0x013f });
        ExpectRegister({ 0x4f, 0x00ef });
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x22 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xf8, 0x00 }, hal::SpiAction::stop));
    }
    std::array<uint8_t, 2> red{ 0xf8, 0x00 };
    infra::VerifyingFunction<void()> done;

    board->Display().Write({ 0, 0, 1, 1 }, red, done);
    ExecuteAllActions();
}

TEST_F(BoostxlK350qvgS1SetupTest, the_reset_line_is_pulsed_low_before_the_bring_up)
{
    EXPECT_CALL(initialized, callback()).Times(testing::AtLeast(0));
    EXPECT_CALL(spi, SendDataMock(testing::_, testing::_)).Times(testing::AtLeast(0));
    board.emplace(spi, dataCommand, reset, [this]()
        {
            initialized.callback();
        });

    EXPECT_FALSE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(10));
    EXPECT_TRUE(reset.GetStubState());
}
