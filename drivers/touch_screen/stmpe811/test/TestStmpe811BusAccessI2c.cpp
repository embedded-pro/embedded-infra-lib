#include "drivers/touch_screen/stmpe811/Stmpe811BusAccessI2c.hpp"
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

    constexpr hal::I2cAddress addressAddr0Low{ 0x41 };
    constexpr hal::I2cAddress addressAddr0High{ 0x44 };

    class Stmpe811BusAccessI2cTest
        : public testing::Test
    {
    public:
        testing::StrictMock<hal::I2cMasterMock> i2c;
        drivers::Stmpe811BusAccessI2c bus{ i2c };
    };
}

TEST_F(Stmpe811BusAccessI2cTest, the_two_selectable_addresses_are_the_7_bit_addresses_of_the_device)
{
    EXPECT_TRUE(drivers::Stmpe811BusAccessI2c::addressAddr0Low == addressAddr0Low);
    EXPECT_TRUE(drivers::Stmpe811BusAccessI2c::addressAddr0High == addressAddr0High);
}

TEST_F(Stmpe811BusAccessI2cTest, a_write_sends_the_register_and_then_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(addressAddr0Low, hal::Action::continueSession, Bytes{ 0x4b }));
    EXPECT_CALL(i2c, SendDataMock(addressAddr0Low, hal::Action::stop, Bytes{ 0x01 }));
    const std::array<uint8_t, 1> data{ 0x01 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x4b, infra::MakeByteRange(data), done);
}

TEST_F(Stmpe811BusAccessI2cTest, a_read_sends_the_register_with_a_repeated_start_and_then_receives_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(addressAddr0Low, hal::Action::repeatedStart, Bytes{ 0x00 }));
    EXPECT_CALL(i2c, ReceiveDataMock(addressAddr0Low, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 0x08, 0x11 }));
    std::array<uint8_t, 2> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x00, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 2>{ 0x08, 0x11 }), data);
}

TEST_F(Stmpe811BusAccessI2cTest, a_read_of_the_register_above_0x7f_keeps_the_whole_register)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(addressAddr0Low, hal::Action::repeatedStart, Bytes{ 0xd7 }));
    EXPECT_CALL(i2c, ReceiveDataMock(addressAddr0Low, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 1, 2, 3, 4 }));
    std::array<uint8_t, 4> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0xd7, infra::MakeByteRange(data), done);

    EXPECT_EQ((std::array<uint8_t, 4>{ 1, 2, 3, 4 }), data);
}

TEST(Stmpe811BusAccessI2cAddressTest, the_device_is_addressed_with_the_address_it_was_given)
{
    testing::StrictMock<hal::I2cMasterMock> i2c;
    drivers::Stmpe811BusAccessI2c bus{ i2c, drivers::Stmpe811BusAccessI2c::addressAddr0High };

    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(addressAddr0High, hal::Action::continueSession, Bytes{ 0x03 }));
    EXPECT_CALL(i2c, SendDataMock(addressAddr0High, hal::Action::stop, Bytes{ 0x02 }));
    const std::array<uint8_t, 1> data{ 0x02 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x03, infra::MakeByteRange(data), done);
}
