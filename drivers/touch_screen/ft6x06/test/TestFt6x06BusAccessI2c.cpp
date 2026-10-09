#include "drivers/touch_screen/ft6x06/Ft6x06BusAccessI2c.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;

    constexpr hal::I2cAddress defaultAddress{ 0x38 };

    class Ft6x06BusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterMock> i2c;
        drivers::Ft6x06BusAccessI2c bus{ i2c };
    };
}

TEST_F(Ft6x06BusAccessI2cTest, the_default_address_is_the_7_bit_address_of_the_device)
{
    EXPECT_TRUE(drivers::Ft6x06BusAccessI2c::defaultAddress == defaultAddress);
}

TEST_F(Ft6x06BusAccessI2cTest, a_write_sends_the_register_and_then_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(defaultAddress, hal::Action::continueSession, Bytes{ 0xa4 }));
    EXPECT_CALL(i2c, SendDataMock(defaultAddress, hal::Action::stop, Bytes{ 0x00 }));
    const std::array<uint8_t, 1> data{ 0x00 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0xa4, infra::MakeByteRange(data), done);
}

TEST_F(Ft6x06BusAccessI2cTest, a_read_sends_the_register_with_a_repeated_start_and_then_receives_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(defaultAddress, hal::Action::repeatedStart, Bytes{ 0x02 }));
    EXPECT_CALL(i2c, ReceiveDataMock(defaultAddress, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 1, 2, 3, 4, 5 }));
    std::array<uint8_t, 5> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x02, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 5>{ 1, 2, 3, 4, 5 }), data);
}

TEST_F(Ft6x06BusAccessI2cTest, a_read_of_a_register_above_0x7f_keeps_the_whole_register)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(defaultAddress, hal::Action::repeatedStart, Bytes{ 0xa8 }));
    EXPECT_CALL(i2c, ReceiveDataMock(defaultAddress, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 0x11 }));
    std::array<uint8_t, 1> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0xa8, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 1>{ 0x11 }), data);
}

TEST(Ft6x06BusAccessI2cAddressTest, the_device_is_addressed_with_the_address_it_was_given)
{
    constexpr hal::I2cAddress otherAddress{ 0x39 };
    testing::StrictMock<hal::I2cMasterMock> i2c;
    drivers::Ft6x06BusAccessI2c bus{ i2c, otherAddress };

    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(otherAddress, hal::Action::continueSession, Bytes{ 0x03 }));
    EXPECT_CALL(i2c, SendDataMock(otherAddress, hal::Action::stop, Bytes{ 0x02 }));
    const std::array<uint8_t, 1> data{ 0x02 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x03, infra::MakeByteRange(data), done);
}
