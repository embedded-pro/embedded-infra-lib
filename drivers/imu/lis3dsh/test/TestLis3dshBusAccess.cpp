#include "drivers/imu/lis3dsh/Lis3dshBusAccess.hpp"
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
    class Lis3dshBusAccessSpiTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        testing::StrictMock<hal::SpiMock> spi;
        drivers::Lis3dshBusAccessSpi bus{ spi };
    };

    class Lis3dshBusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::Lis3dshBusAccessI2c bus{ i2c };
    };

    TEST_F(Lis3dshBusAccessSpiTest, single_byte_read_sets_bit_seven_only)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x8f }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        ExecuteAllActions();

        EXPECT_EQ(0x3f, value);
    }

    TEST_F(Lis3dshBusAccessSpiTest, burst_read_sets_bit_seven_only_not_0x40)
    {
        // LIS3DSH increments via CTRL_REG6.ADD_INC, not a multi-byte flag in the address byte
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0xa8 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, ReceiveDataMock(hal::SpiAction::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Lis3dshBusAccessSpiTest, single_byte_write_clears_bit_seven)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x20 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x6f }, hal::SpiAction::stop));

        const uint8_t value = 0x6f;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshBusAccessSpiTest, burst_write_does_not_set_0x40)
    {
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 0x28 }, hal::SpiAction::continueSession));
        EXPECT_CALL(spi, SendDataMock(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }, hal::SpiAction::stop));

        const std::array<uint8_t, 6> data{ { 1, 2, 3, 4, 5, 6 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x28, infra::MakeByteRange(data), done);

        ExecuteAllActions();
    }

    TEST_F(Lis3dshBusAccessI2cTest, single_byte_read_passes_address_unchanged)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        EXPECT_EQ(0x3f, value);
    }

    TEST_F(Lis3dshBusAccessI2cTest, burst_read_passes_address_unchanged_no_auto_increment_flag)
    {
        // LIS3DSH I2C: no sub-address flag, increment via CTRL_REG6.ADD_INC
        EXPECT_CALL(i2c, ReadRegisterMock(0x28)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(Lis3dshBusAccessI2cTest, single_byte_write_passes_address_unchanged)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x6f }));

        const uint8_t value = 0x6f;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
    }

    TEST_F(Lis3dshBusAccessI2cTest, default_i2c_address_is_sel_low)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x1e }, drivers::Lis3dshBusAccessI2c::addressSelLow);
    }

    TEST_F(Lis3dshBusAccessI2cTest, sel_high_i2c_address_is_0x1d)
    {
        EXPECT_EQ(hal::I2cAddress{ 0x1d }, drivers::Lis3dshBusAccessI2c::addressSelHigh);
    }

    TEST_F(Lis3dshBusAccessI2cTest, explicit_sel_high_address_is_accepted)
    {
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2cHigh;
        drivers::Lis3dshBusAccessI2c busHigh{ i2cHigh, drivers::Lis3dshBusAccessI2c::addressSelHigh };

        EXPECT_CALL(i2cHigh, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x3f }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        busHigh.ReadRegister(0x0f, infra::MakeByteRange(value), done);
    }
}
