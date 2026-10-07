#include "boards/mb1166/Mb1166Setup.hpp"
#include "hal/interfaces/test_doubles/DsiHostMock.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
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
    using InitializationResult = drivers::MipiDsiPanelCore::InitializationResult;

    struct ExpectedCommand
    {
        uint8_t command;
        std::vector<uint8_t> parameters;
        uint16_t delayAfterInMilliseconds;
    };

    const std::vector<ExpectedCommand> expectedBeforeSleepOut{
        { 0x00, { 0x00 }, 0 },
        { 0xff, { 0x80, 0x09, 0x01 }, 0 },
        { 0x00, { 0x80 }, 0 },
        { 0xff, { 0x80, 0x09 }, 0 },
        { 0x00, { 0x80 }, 0 },
        { 0xc4, { 0x30 }, 10 },
        { 0x00, { 0x8a }, 0 },
        { 0xc4, { 0x40 }, 10 },
        { 0x00, { 0xb1 }, 0 },
        { 0xc5, { 0xa9 }, 0 },
        { 0x00, { 0x91 }, 0 },
        { 0xc5, { 0x34 }, 0 },
        { 0x00, { 0xb4 }, 0 },
        { 0xc0, { 0x50 }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0xd9, { 0x4e }, 0 },
        { 0x00, { 0x81 }, 0 },
        { 0xc1, { 0x66 }, 0 },
        { 0x00, { 0xa1 }, 0 },
        { 0xc1, { 0x08 }, 0 },
        { 0x00, { 0x92 }, 0 },
        { 0xc5, { 0x01 }, 0 },
        { 0x00, { 0x95 }, 0 },
        { 0xc5, { 0x34 }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0xd8, { 0x79, 0x79 }, 0 },
        { 0x00, { 0x94 }, 0 },
        { 0xc5, { 0x33 }, 0 },
        { 0x00, { 0xa3 }, 0 },
        { 0xc0, { 0x1b }, 0 },
        { 0x00, { 0x82 }, 0 },
        { 0xc5, { 0x83 }, 0 },
        { 0x00, { 0x81 }, 0 },
        { 0xc4, { 0x83 }, 0 },
        { 0x00, { 0xa1 }, 0 },
        { 0xc1, { 0x0e }, 0 },
        { 0x00, { 0xa6 }, 0 },
        { 0xb3, { 0x00, 0x01 }, 0 },
        { 0x00, { 0x80 }, 0 },
        { 0xce, { 0x85, 0x01, 0x00, 0x84, 0x01, 0x00 }, 0 },
        { 0x00, { 0xa0 }, 0 },
        { 0xce, { 0x18, 0x04, 0x03, 0x39, 0x00, 0x00, 0x00, 0x18, 0x03, 0x03, 0x3a, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xb0 }, 0 },
        { 0xce, { 0x18, 0x02, 0x03, 0x3b, 0x00, 0x00, 0x00, 0x18, 0x01, 0x03, 0x3c, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xc0 }, 0 },
        { 0xcf, { 0x01, 0x01, 0x20, 0x20, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00 }, 0 },
        { 0x00, { 0xd0 }, 0 },
        { 0xcf, { 0x00 }, 0 },
        { 0x00, { 0x80 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0x90 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xa0 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xb0 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xc0 }, 0 },
        { 0xcb, { 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xd0 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xe0 }, 0 },
        { 0xcb, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xf0 }, 0 },
        { 0xcb, { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0 },
        { 0x00, { 0x80 }, 0 },
        { 0xcc, { 0x00, 0x26, 0x09, 0x0b, 0x01, 0x25, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0x90 }, 0 },
        { 0xcc, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x0a, 0x0c, 0x02 }, 0 },
        { 0x00, { 0xa0 }, 0 },
        { 0xcc, { 0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xb0 }, 0 },
        { 0xcc, { 0x00, 0x25, 0x0c, 0x0a, 0x02, 0x26, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0xc0 }, 0 },
        { 0xcc, { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x25, 0x0b, 0x09, 0x01 }, 0 },
        { 0x00, { 0xd0 }, 0 },
        { 0xcc, { 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 },
        { 0x00, { 0x81 }, 0 },
        { 0xc5, { 0x66 }, 0 },
        { 0x00, { 0xb6 }, 0 },
        { 0xf5, { 0x06 }, 0 },
        { 0x00, { 0xb1 }, 0 },
        { 0xc6, { 0x06 }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0xff, { 0xff, 0xff, 0xff }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0xe1, { 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a, 0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01 }, 0 },
        { 0x00, { 0x00 }, 0 },
        { 0xe2, { 0x00, 0x09, 0x0f, 0x0e, 0x07, 0x10, 0x0b, 0x0a, 0x04, 0x07, 0x0b, 0x08, 0x0f, 0x10, 0x0a, 0x01 }, 0 },
    };

    const std::vector<ExpectedCommand> expectedAfterSleepOut{
        { 0x2a, { 0x00, 0x00, 0x03, 0x1f }, 0 },
        { 0x2b, { 0x00, 0x00, 0x01, 0xdf }, 0 },
        { 0x51, { 0x7f }, 0 },
        { 0x53, { 0x2c }, 0 },
        { 0x55, { 0x02 }, 0 },
        { 0x5e, { 0xff }, 0 },
    };

    void ExpectEqual(const std::vector<ExpectedCommand>& expected, infra::MemoryRange<const drivers::MipiDsiPanelCore::Command> actual)
    {
        ASSERT_EQ(expected.size(), actual.size());

        for (std::size_t i = 0; i != expected.size(); ++i)
        {
            EXPECT_EQ(drivers::MipiDsiPanelCore::Packet::dcs, actual[i].packet) << i;
            EXPECT_EQ(expected[i].command, actual[i].command) << i;
            EXPECT_EQ(expected[i].parameters, std::vector<uint8_t>(actual[i].parameters.begin(), actual[i].parameters.end())) << i;
            EXPECT_EQ(expected[i].delayAfterInMilliseconds, actual[i].delayAfterInMilliseconds) << i;
        }
    }

    class Mb1166SetupTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectDcs(uint8_t command, std::vector<uint8_t> parameters)
        {
            EXPECT_CALL(host, WriteDcsMock(command, parameters));
        }

        void ExpectCommands(const std::vector<ExpectedCommand>& commands)
        {
            for (const auto& command : commands)
                ExpectDcs(command.command, command.parameters);
        }

        void Create(hal::PixelFormat format = hal::PixelFormat::rgb565)
        {
            board.emplace(host, stream, reset, format, [this](InitializationResult result)
                {
                    initialized.callback(result);
                });
        }

        testing::StrictMock<hal::DsiHostMock> host;
        testing::StrictMock<hal::DsiVideoStreamMock> stream;
        hal::GpioPinStub reset;
        testing::StrictMock<infra::MockCallback<void(InitializationResult)>> initialized;
        std::optional<boards::Mb1166Setup> board;
    };
}

TEST(Mb1166PanelTest, the_panel_is_800_by_480_in_landscape)
{
    EXPECT_EQ((hal::DisplaySize{ 800, 480 }), boards::mb1166Panel.size);
    EXPECT_EQ(0x60, boards::mb1166Panel.addressMode);
}

TEST(Mb1166PanelTest, the_panel_is_not_identified_by_reading_it)
{
    EXPECT_TRUE(boards::mb1166Panel.identification.expected.empty());
}

TEST(Mb1166PanelTest, the_controller_is_reset_for_20_ms_and_given_10_ms_to_recover)
{
    EXPECT_EQ(std::chrono::milliseconds(20), boards::mb1166Panel.timings.resetPulse);
    EXPECT_EQ(std::chrono::milliseconds(10), boards::mb1166Panel.timings.resetRecovery);
}

TEST(Mb1166PanelTest, the_commands_before_sleep_out_are_those_of_the_board_support_package)
{
    ExpectEqual(expectedBeforeSleepOut, boards::mb1166Panel.beforeSleepOut);
}

TEST(Mb1166PanelTest, the_commands_after_sleep_out_are_those_of_the_board_support_package)
{
    ExpectEqual(expectedAfterSleepOut, boards::mb1166Panel.afterSleepOut);
}

TEST_F(Mb1166SetupTest, initialization_runs_the_tables_around_sleep_out_and_starts_the_video_stream_before_display_on)
{
    {
        testing::InSequence sequence;
        ExpectCommands(expectedBeforeSleepOut);
        ExpectDcs(0x11, {});
        ExpectDcs(0x3a, { 0x55 });
        ExpectDcs(0x36, { 0x60 });
        ExpectCommands(expectedAfterSleepOut);
        EXPECT_CALL(stream, StartMock());
        ExpectDcs(0x29, {});
        EXPECT_CALL(initialized, callback(InitializationResult::success));
    }

    Create();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Mb1166SetupTest, the_reset_line_is_pulsed_low_before_the_first_command)
{
    Create();

    EXPECT_FALSE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(19));
    EXPECT_FALSE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(1));
    EXPECT_TRUE(reset.GetStubState());
    ForwardTime(std::chrono::milliseconds(9));

    testing::Mock::VerifyAndClearExpectations(&host);
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(stream, StartMock());
    EXPECT_CALL(initialized, callback(InitializationResult::success));
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Mb1166SetupTest, the_panel_can_be_put_to_sleep_through_the_setup)
{
    EXPECT_CALL(host, WriteDcsMock(testing::_, testing::_)).Times(testing::AnyNumber());
    EXPECT_CALL(stream, StartMock());
    EXPECT_CALL(initialized, callback(InitializationResult::success));
    Create();
    ForwardTime(std::chrono::seconds(1));
    testing::Mock::VerifyAndClearExpectations(&host);
    testing::Mock::VerifyAndClearExpectations(&stream);

    {
        testing::InSequence sequence;
        ExpectDcs(0x28, {});
        EXPECT_CALL(stream, StopMock());
        ExpectDcs(0x10, {});
    }
    infra::VerifyingFunction<void()> done;

    board->Panel().Sleep(done);
    ForwardTime(std::chrono::seconds(1));
}
