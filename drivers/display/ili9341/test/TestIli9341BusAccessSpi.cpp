#include "drivers/display/ili9341/Ili9341BusAccessSpi.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <vector>

namespace
{
    constexpr bool commandLevel = false;
    constexpr bool dataLevel = true;

    class Ili9341BusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectSend(std::vector<uint8_t> bytes, hal::SpiAction nextAction, bool expectedLevel)
        {
            EXPECT_CALL(spi, SendDataMock(bytes, nextAction)).WillOnce(testing::Invoke([this, expectedLevel](std::vector<uint8_t>, hal::SpiAction)
                {
                    EXPECT_EQ(expectedLevel, dataCommand.GetStubState());
                }));
        }

        testing::StrictMock<hal::SpiMock> spi;
        hal::GpioPinStub dataCommand;
        drivers::Ili9341BusAccessSpi bus{ spi, dataCommand };
    };
}

TEST_F(Ili9341BusAccessSpiTest, data_command_line_idles_at_the_data_level)
{
    EXPECT_EQ(dataLevel, dataCommand.GetStubState());
}

TEST_F(Ili9341BusAccessSpiTest, the_register_index_is_sent_at_the_command_level_and_continues_the_session)
{
    testing::InSequence sequence;
    ExpectSend({ 0x22 }, hal::SpiAction::continueSession, commandLevel);
    ExpectSend({ 0x12, 0x34 }, hal::SpiAction::stop, dataLevel);
    std::array<uint8_t, 2> data{ 0x12, 0x34 };

    bus.WriteRegister(0x22, data, [] {});
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, a_register_without_data_is_a_single_transfer_that_stops_the_session)
{
    ExpectSend({ 0x07 }, hal::SpiAction::stop, commandLevel);

    bus.WriteRegister(0x07, infra::ConstByteRange(), [] {});
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, the_data_line_returns_to_the_data_level_after_the_index_was_sent)
{
    ExpectSend({ 0x07 }, hal::SpiAction::stop, commandLevel);
    bus.WriteRegister(0x07, infra::ConstByteRange(), [] {});

    ExecuteAllActions();

    EXPECT_EQ(dataLevel, dataCommand.GetStubState());
}

TEST_F(Ili9341BusAccessSpiTest, the_whole_data_range_is_sent_in_one_transfer)
{
    testing::InSequence sequence;
    ExpectSend({ 0x22 }, hal::SpiAction::continueSession, commandLevel);
    ExpectSend({ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, hal::SpiAction::stop, dataLevel);
    std::array<uint8_t, 10> data{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };

    bus.WriteRegister(0x22, data, [] {});
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, completion_is_reported_once_after_the_data_was_sent)
{
    ExpectSend({ 0x22 }, hal::SpiAction::continueSession, commandLevel);
    ExpectSend({ 0xff }, hal::SpiAction::stop, dataLevel);
    std::array<uint8_t, 1> data{ 0xff };
    infra::VerifyingFunction<void()> done;

    bus.WriteRegister(0x22, data, done);
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, completion_is_not_reported_before_the_transfers_are_done)
{
    ExpectSend({ 0x22 }, hal::SpiAction::continueSession, commandLevel);
    ExpectSend({ 0xff }, hal::SpiAction::stop, dataLevel);
    std::array<uint8_t, 1> data{ 0xff };
    testing::StrictMock<infra::MockCallback<void()>> done;

    bus.WriteRegister(0x22, data, [&done]()
        {
            done.callback();
        });

    testing::Mock::VerifyAndClearExpectations(&done);
    EXPECT_CALL(done, callback());
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, a_new_write_can_be_started_from_the_completion_callback)
{
    testing::InSequence sequence;
    ExpectSend({ 0x07 }, hal::SpiAction::stop, commandLevel);
    ExpectSend({ 0x10 }, hal::SpiAction::stop, commandLevel);

    bus.WriteRegister(0x07, infra::ConstByteRange(), [this]()
        {
            bus.WriteRegister(0x10, infra::ConstByteRange(), [] {});
        });
    ExecuteAllActions();
}

TEST_F(Ili9341BusAccessSpiTest, a_write_while_another_is_in_flight_asserts)
{
    EXPECT_CALL(spi, SendDataMock(testing::_, testing::_)).Times(testing::AtLeast(1));
    bus.WriteRegister(0x07, infra::ConstByteRange(), [] {});

    EXPECT_DEATH(bus.WriteRegister(0x10, infra::ConstByteRange(), [] {}), "");
}

TEST_F(Ili9341BusAccessSpiTest, reading_a_register_asserts_because_the_data_output_is_not_connected)
{
    std::array<uint8_t, 2> data{};

    EXPECT_DEATH(bus.ReadRegister(0x00, data, [] {}), "");
}
