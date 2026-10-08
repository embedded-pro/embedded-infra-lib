#include "drivers/imu/common/ImuBusAccessI2c.hpp"
#include "hal/interfaces/test_doubles/I2cRegisterAccessMock.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <vector>

namespace
{
    class ImuBusAccessI2cWithAutoIncrementTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::ImuBusAccessI2c bus{ i2c, hal::I2cAddress{ 0x1c }, 0x80 };
    };

    class ImuBusAccessI2cWithoutAutoIncrementTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterRegisterAccessMock> i2c;
        drivers::ImuBusAccessI2c bus{ i2c, hal::I2cAddress{ 0x1c }, 0 };
    };

    TEST_F(ImuBusAccessI2cWithAutoIncrementTest, single_byte_read_leaves_the_auto_increment_bit_clear)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x0f)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xd4 }));

        uint8_t value = 0;
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x0f, infra::MakeByteRange(value), done);

        EXPECT_EQ(0xd4, value);
    }

    TEST_F(ImuBusAccessI2cWithAutoIncrementTest, burst_read_sets_the_auto_increment_bit)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0xa8)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(ImuBusAccessI2cWithAutoIncrementTest, single_byte_write_leaves_the_auto_increment_bit_clear)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 0x57 }));

        const uint8_t value = 0x57;
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(value), done);
    }

    TEST_F(ImuBusAccessI2cWithAutoIncrementTest, burst_write_sets_the_auto_increment_bit)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0xa0, std::vector<uint8_t>{ 1, 2 }));

        const std::array<uint8_t, 2> data{ { 1, 2 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(data), done);
    }

    TEST_F(ImuBusAccessI2cWithoutAutoIncrementTest, burst_read_without_flag_does_not_set_any_bit)
    {
        EXPECT_CALL(i2c, ReadRegisterMock(0x28)).WillOnce(testing::Return(std::vector<uint8_t>{ 1, 2, 3, 4, 5, 6 }));

        std::array<uint8_t, 6> data{};
        infra::VerifyingFunction<void()> done;
        bus.ReadRegister(0x28, infra::MakeByteRange(data), done);

        EXPECT_EQ((std::array<uint8_t, 6>{ { 1, 2, 3, 4, 5, 6 } }), data);
    }

    TEST_F(ImuBusAccessI2cWithoutAutoIncrementTest, burst_write_without_flag_does_not_set_any_bit)
    {
        EXPECT_CALL(i2c, WriteRegisterMock(0x20, std::vector<uint8_t>{ 1, 2 }));

        const std::array<uint8_t, 2> data{ { 1, 2 } };
        infra::VerifyingFunction<void()> done;
        bus.WriteRegister(0x20, infra::MakeByteRange(data), done);
    }
}
