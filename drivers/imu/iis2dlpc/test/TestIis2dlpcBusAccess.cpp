#include "drivers/imu/iis2dlpc/Iis2dlpcBusAccess.hpp"
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
    class Iis2dlpcBusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::Iis2dlpcBusAccessSpi bus{ spi };
    };

    class Iis2dlpcBusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Iis2dlpcBusAccessI2c bus{ i2c };
    };

    TEST_F(Iis2dlpcBusAccessSpiTest, single_byte_read_sets_bit_seven_only)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x8f }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0x44, value);
    }

    TEST_F(Iis2dlpcBusAccessSpiTest, burst_read_of_six_bytes_sets_read_flag_only_not_0x40)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xa8 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Iis2dlpcBusAccessSpiTest, single_byte_write_sends_address_with_bit_seven_clear)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x54 }, hal::SpiAction::stop));

        const uint8_t value = 0x54;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);

        ExecuteAllActions();
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, single_byte_read_passes_plain_register_address)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        EXPECT_EQ(0x44, value);
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, burst_read_passes_plain_register_address_no_auto_increment_flag)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x28)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, single_byte_write_passes_plain_register_address)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x54 }));

        const uint8_t value = 0x54;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, default_i2c_address_is_sa0_low_0x18)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x18 }, drivers::Iis2dlpcBusAccessI2c::addressSa0Low);
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, sa0_high_i2c_address_is_0x19)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x19 }, drivers::Iis2dlpcBusAccessI2c::addressSa0High);
    }

    TEST_F(Iis2dlpcBusAccessI2cTest, explicit_sa0_high_address_is_accepted)
    {
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2cHigh;
        drivers::Iis2dlpcBusAccessI2c busHigh{ i2cHigh, drivers::Iis2dlpcBusAccessI2c::addressSa0High };

        EXPECT_CALL(i2cHigh, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x44 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        busHigh.ReadRegister(0x0f, infra::MakeByteRange(value), done);
    }
}
