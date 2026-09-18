#include "drivers/imu/l3gd20/L3gd20BusAccessI2c.hpp"
#include "drivers/imu/l3gd20/L3gd20BusAccessSpi.hpp"
#include "hal/interfaces/test_doubles/I2cRegisterAccessMock.hpp"
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
    class L3gd20BusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::L3gd20BusAccessI2c bus{ i2c };
    };

    class L3gd20BusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::L3gd20BusAccessSpi bus{ spi };
    };

    TEST_F(L3gd20BusAccessI2cTest, a_single_byte_read_leaves_the_auto_increment_bit_clear)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        EXPECT_EQ(0xd4, value);
    }

    TEST_F(L3gd20BusAccessI2cTest, a_burst_read_sets_the_auto_increment_bit)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0xa8)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(L3gd20BusAccessI2cTest, a_single_byte_write_leaves_the_auto_increment_bit_clear)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x0f }));

        const uint8_t value = 0x0f;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
    }

    TEST_F(L3gd20BusAccessI2cTest, a_burst_write_sets_the_auto_increment_bit)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0xa0, std::vector<uint8_t>{ 1, 2 }));

        const std::array<uint8_t, 2> data{ { 1, 2 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(data), done);
    }

    TEST_F(L3gd20BusAccessI2cTest, the_slave_address_is_selected_by_the_sdo_pin)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x6a }, drivers::L3gd20BusAccessI2c::addressSdoLow);
        EXPECT_EQ(hal::I2cAddress{ 0x6b }, drivers::L3gd20BusAccessI2c::addressSdoHigh);
    }

    TEST_F(L3gd20BusAccessSpiTest, read_sets_bit_seven_of_the_command_byte)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x8f }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd7 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0xd7, value);
    }

    TEST_F(L3gd20BusAccessSpiTest, write_clears_bit_seven_of_the_command_byte)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x0f }, hal::SpiAction::stop));

        const uint8_t value = 0x0f;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);

        ExecuteAllActions();
    }

    // Without the multiple byte bit the device re-addresses the same register for every clocked
    // byte, so a burst of the output registers would return six copies of the low byte of X
    TEST_F(L3gd20BusAccessSpiTest, a_burst_read_sets_the_multiple_byte_bit_and_receives_all_bytes_in_one_session)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xe8 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(L3gd20BusAccessSpiTest, a_burst_write_sets_the_multiple_byte_bit)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x72 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }, hal::SpiAction::stop));

        const std::array<uint8_t, 6> data{ { 1, 2, 3, 4, 5, 6 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x32, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(L3gd20BusAccessSpiTest, consecutive_transfers_are_accepted)
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
