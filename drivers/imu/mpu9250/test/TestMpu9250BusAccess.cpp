#include "drivers/imu/mpu9250/Mpu9250BusAccessI2c.hpp"
#include "drivers/imu/mpu9250/Mpu9250BusAccessSpi.hpp"
#include "hal/interfaces/test_doubles/I2cRegisterAccessMock.hpp"
#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    class Mpu9250BusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Mpu9250BusAccessI2c bus{ i2c };
    };

    class Mpu9250BusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::Mpu9250BusAccessSpi bus{ spi };
    };
}

TEST_F(Mpu9250BusAccessI2cTest, read_register_addresses_the_requested_register)
{
    EXPECT_CALL(i2c, ReadRegisterMock(0x75)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x71 }));

    uint8_t value = 0;
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x75, infra::MakeByteRange(value), done);

    EXPECT_EQ(0x71, value);
}

TEST_F(Mpu9250BusAccessI2cTest, burst_read_uses_auto_increment_from_one_register)
{
    EXPECT_CALL(i2c, ReadRegisterMock(0x3B)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

    std::array<uint8_t, 6> data{};
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x3B, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
}

TEST_F(Mpu9250BusAccessI2cTest, write_register_sends_register_then_payload)
{
    EXPECT_CALL(i2c, WriteRegisterMock(0x6B, std::vector<uint8_t>{ 0x80 }));

    const uint8_t value = 0x80;
    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x6B, infra::MakeByteRange(value), done);
}

TEST_F(Mpu9250BusAccessSpiTest, read_sets_bit_seven_of_the_address)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xF5 }, hal::SpiAction::continueSession));
    EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x71 }));

    uint8_t value = 0;
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x75, infra::MakeByteRange(value), done);

    ExecuteAllActions();

    EXPECT_EQ(0x71, value);
}

TEST_F(Mpu9250BusAccessSpiTest, write_clears_bit_seven_of_the_address)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x6B }, hal::SpiAction::continueSession));
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x80 }, hal::SpiAction::stop));

    const uint8_t value = 0x80;
    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x6B, infra::MakeByteRange(value), done);

    ExecuteAllActions();
}

TEST_F(Mpu9250BusAccessSpiTest, burst_read_receives_all_bytes_in_one_session)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xBB }, hal::SpiAction::continueSession));
    EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

    std::array<uint8_t, 6> data{};
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x3B, infra::MakeByteRange(data), done);

    ExecuteAllActions();

    EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
}

TEST_F(Mpu9250BusAccessSpiTest, consecutive_transfers_are_accepted)
{
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x1B }, hal::SpiAction::continueSession));
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x18 }, hal::SpiAction::stop));

    const uint8_t first = 0x18;
    infra::VerifyingFunction<void()> firstDone;
    bus.WriteRegister(0x1B, infra::MakeByteRange(first), firstDone);

    ExecuteAllActions();

    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x1C }, hal::SpiAction::continueSession));
    EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x08 }, hal::SpiAction::stop));

    const uint8_t second = 0x08;
    infra::VerifyingFunction<void()> secondDone;
    bus.WriteRegister(0x1C, infra::MakeByteRange(second), secondDone);

    ExecuteAllActions();
}

TEST_F(Mpu9250BusAccessI2cTest, does_not_require_the_i2c_slave_interface_to_be_disabled)
{
    EXPECT_FALSE(bus.RequiresI2cSlaveInterfaceDisabled());
}

TEST_F(Mpu9250BusAccessSpiTest, requires_the_i2c_slave_interface_to_be_disabled)
{
    EXPECT_TRUE(bus.RequiresI2cSlaveInterfaceDisabled());
}
