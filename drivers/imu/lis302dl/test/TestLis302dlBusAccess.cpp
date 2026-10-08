#include "drivers/imu/lis302dl/Lis302dlBusAccess.hpp"
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
    class Lis302dlBusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::Lis302dlBusAccessSpi bus{ spi };
    };

    class Lis302dlBusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Lis302dlBusAccessI2c bus{ i2c };
    };

    TEST_F(Lis302dlBusAccessSpiTest, single_byte_read_sets_read_flag_only)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x8f }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0x3b, value);
    }

    TEST_F(Lis302dlBusAccessSpiTest, burst_read_sets_read_flag_and_multi_byte_flag)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xe9 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x29, infra::MakeByteRange(data), done);

        ExecuteAllActions();

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Lis302dlBusAccessSpiTest, single_byte_write_sends_address_without_flags)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x47 }, hal::SpiAction::stop));

        const uint8_t value = 0x47;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlBusAccessSpiTest, multi_byte_write_sets_multi_byte_flag)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x69 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }, hal::SpiAction::stop));

        const std::array<uint8_t, 6> data{ { 1, 2, 3, 4, 5, 6 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x29, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(Lis302dlBusAccessI2cTest, single_byte_read_uses_plain_sub_address)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        EXPECT_EQ(0x3b, value);
    }

    TEST_F(Lis302dlBusAccessI2cTest, burst_read_sets_auto_increment_bit_in_sub_address)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0xa9)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x29, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Lis302dlBusAccessI2cTest, single_byte_write_uses_plain_address)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x47 }));

        const uint8_t value = 0x47;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
    }

    TEST_F(Lis302dlBusAccessI2cTest, multi_byte_write_sets_auto_increment_bit_in_sub_address)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0xa9, std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        const std::array<uint8_t, 6> data{ { 1, 2, 3, 4, 5, 6 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x29, infra::MakeByteRange(data), done);
    }

    TEST_F(Lis302dlBusAccessI2cTest, default_i2c_address_is_sdo_low)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x1c }, drivers::Lis302dlBusAccessI2c::addressSdoLow);
    }

    TEST_F(Lis302dlBusAccessI2cTest, sdo_high_i2c_address_is_0x1d)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x1d }, drivers::Lis302dlBusAccessI2c::addressSdoHigh);
    }

    TEST_F(Lis302dlBusAccessI2cTest, explicit_sdo_high_address_is_accepted)
    {
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2cHigh;
        drivers::Lis302dlBusAccessI2c busHigh{ i2cHigh, drivers::Lis302dlBusAccessI2c::addressSdoHigh };

        EXPECT_CALL(i2cHigh, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3b }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        busHigh.ReadRegister(0x0f, infra::MakeByteRange(value), done);
    }
}
