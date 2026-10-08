#include "drivers/imu/common/ImuBusAccessSpi.hpp"
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
    class ImuBusAccessSpiWithMultipleByteTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::ImuBusAccessSpi bus{ spi, 0x40 };
    };

    class ImuBusAccessSpiWithoutMultipleByteTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::ImuBusAccessSpi bus{ spi, 0 };
    };

    TEST_F(ImuBusAccessSpiWithMultipleByteTest, single_byte_read_sets_bit_seven_of_the_command_byte)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x8f }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd7 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0xd7, value);
    }

    TEST_F(ImuBusAccessSpiWithMultipleByteTest, burst_read_sets_bit_seven_and_multiple_byte_flag)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xe8 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(ImuBusAccessSpiWithMultipleByteTest, single_byte_write_clears_bit_seven_of_the_command_byte)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x0f }, hal::SpiAction::stop));

        const uint8_t value = 0x0f;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);

        ExecuteAllActions();
    }

    TEST_F(ImuBusAccessSpiWithMultipleByteTest, burst_write_sets_the_multiple_byte_flag)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x72 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }, hal::SpiAction::stop));

        const std::array<uint8_t, 6> data{ { 1, 2, 3, 4, 5, 6 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x32, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(ImuBusAccessSpiWithoutMultipleByteTest, burst_read_without_flag_does_not_set_multiple_byte_bit)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xa8 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(ImuBusAccessSpiWithoutMultipleByteTest, burst_write_without_flag_does_not_set_multiple_byte_bit)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x32 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 1, 2 }, hal::SpiAction::stop));

        const std::array<uint8_t, 2> data{ { 1, 2 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x32, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(ImuBusAccessSpiWithMultipleByteTest, consecutive_transfers_are_accepted)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession)).Times(2);
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x0f }, hal::SpiAction::stop)).Times(2);

        const uint8_t value = 0x0f;
        infra::VerifyingFunction<void()> first;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), first);
        ExecuteAllActions();

        infra::VerifyingFunction<void()> second;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), second);
        ExecuteAllActions();
    }
}
