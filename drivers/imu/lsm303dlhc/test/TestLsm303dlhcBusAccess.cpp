#include "drivers/imu/lsm303dlhc/Lsm303dlhcBusAccessI2c.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "hal/interfaces/test_doubles/I2cRegisterAccessMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "services/peripheral/I2cMultipleAccess.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    class Lsm303dlhcAccelerometerBusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Lsm303dlhcAccelerometerBusAccessI2c bus{ i2c };
    };

    // The two blocks are separate slaves. When they share one master, each register transaction
    // spans a repeated start, so the caller is expected to interpose an I2cMultipleAccess per block
    class Lsm303dlhcSharedMasterTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        Lsm303dlhcSharedMasterTest()
            : multipleAccess(i2c)
            , accelerometerAccess(multipleAccess)
            , magnetometerAccess(multipleAccess)
            , accelerometerBus(accelerometerAccess)
            , magnetometerBus(magnetometerAccess)
        {}

        testing::StrictMock<hal::I2cMasterMockWithoutAutomaticDone> i2c;
        services::I2cMultipleAccessMaster multipleAccess;
        services::I2cMultipleAccess accelerometerAccess;
        services::I2cMultipleAccess magnetometerAccess;
        drivers::Lsm303dlhcAccelerometerBusAccessI2c accelerometerBus;
        drivers::Lsm303dlhcMagnetometerBusAccessI2c magnetometerBus;

        std::array<uint8_t, 6> acceleration{};
        std::array<uint8_t, 6> magneticFluxDensity{};
    };

    class Lsm303dlhcMagnetometerBusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Lsm303dlhcMagnetometerBusAccessI2c bus{ i2c };
    };
}

TEST_F(Lsm303dlhcAccelerometerBusAccessI2cTest, a_single_byte_read_leaves_the_auto_increment_bit_clear)
{
    EXPECT_CALL(i2c, ReadRegisterMock(0x27)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x08 }));

    uint8_t value = 0;
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x27, infra::MakeByteRange(value), done);

    EXPECT_EQ(0x08, value);
}

TEST_F(Lsm303dlhcAccelerometerBusAccessI2cTest, a_burst_read_sets_the_auto_increment_bit)
{
    EXPECT_CALL(i2c, ReadRegisterMock(0xa8)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

    std::array<uint8_t, 6> data{};
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
}

TEST_F(Lsm303dlhcAccelerometerBusAccessI2cTest, a_single_byte_write_leaves_the_auto_increment_bit_clear)
{
    EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));

    const uint8_t value = 0x57;
    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
}

TEST_F(Lsm303dlhcAccelerometerBusAccessI2cTest, a_burst_write_sets_the_auto_increment_bit)
{
    EXPECT_CALL(i2c, WriteRegisterMock(0xa0, std::vector<uint8_t>{ 0x57, 0x00 }));

    const std::array<uint8_t, 2> data{ { 0x57, 0x00 } };
    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x20, infra::MakeByteRange(data), done);
}

TEST_F(Lsm303dlhcAccelerometerBusAccessI2cTest, the_accelerometer_answers_on_its_own_slave_address)
{
    EXPECT_TRUE(drivers::Lsm303dlhcAccelerometerBusAccessI2c::address == hal::I2cAddress{ 0x19 });
}

TEST_F(Lsm303dlhcMagnetometerBusAccessI2cTest, a_burst_read_leaves_the_auto_increment_bit_clear)
{
    EXPECT_CALL(i2c, ReadRegisterMock(0x03)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

    std::array<uint8_t, 6> data{};
    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x03, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
}

TEST_F(Lsm303dlhcMagnetometerBusAccessI2cTest, a_write_addresses_the_requested_register)
{
    EXPECT_CALL(i2c, WriteRegisterMock(0x00, std::vector<uint8_t>{ 0x90 }));

    const uint8_t value = 0x90;
    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x00, infra::MakeByteRange(value), done);
}

TEST_F(Lsm303dlhcMagnetometerBusAccessI2cTest, the_magnetometer_answers_on_its_own_slave_address)
{
    EXPECT_TRUE(drivers::Lsm303dlhcMagnetometerBusAccessI2c::address == hal::I2cAddress{ 0x1e });
}

TEST_F(Lsm303dlhcSharedMasterTest, a_transaction_of_one_block_is_not_interleaved_by_the_other)
{
    EXPECT_CALL(i2c, SendDataMock(hal::I2cAddress{ 0x19 }, hal::Action::repeatedStart, std::vector<uint8_t>{ 0xa8 }));

    bool accelerationDone = false;
    accelerometerBus.ReadRegister(0x28, infra::MakeByteRange(acceleration), [&accelerationDone]()
        {
            accelerationDone = true;
        });

    bool magneticFluxDensityDone = false;
    magnetometerBus.ReadRegister(0x03, infra::MakeByteRange(magneticFluxDensity), [&magneticFluxDensityDone]()
        {
            magneticFluxDensityDone = true;
        });

    // The magnetometer asked while the accelerometer still held the bus, so it did not reach the
    // master; a strict mock fails the test if it had
    ExecuteAllActions();

    EXPECT_CALL(i2c, ReceiveDataMock(hal::I2cAddress{ 0x19 }, hal::Action::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));
    i2c.onSent(hal::Result::complete, 1);
    ExecuteAllActions();

    EXPECT_FALSE(accelerationDone);

    // Finishing the accelerometer session releases the claim and lets the magnetometer through
    EXPECT_CALL(i2c, SendDataMock(hal::I2cAddress{ 0x1e }, hal::Action::repeatedStart, std::vector<uint8_t>{ 0x03 }));
    i2c.onReceived(hal::Result::complete);
    ExecuteAllActions();

    EXPECT_TRUE(accelerationDone);
    EXPECT_FALSE(magneticFluxDensityDone);
}
