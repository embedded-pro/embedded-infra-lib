#include "drivers/camera/omnivision/SccbBusAccessI2c.hpp"
#include "hal/interfaces/I2c.hpp"
#include "hal/interfaces/test_doubles/I2cMock.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <cstdint>
#include <vector>

namespace
{
    constexpr hal::I2cAddress sensorAddress{ 0x21 };

    class SccbBusAccessI2cTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<hal::I2cMasterMock> i2c;
        drivers::SccbBusAccessI2c bus{ i2c, sensorAddress };
    };

    class SccbBusAccessI2cSlowBusTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<hal::I2cMasterMockWithoutAutomaticDone> i2c;
        drivers::SccbBusAccessI2c bus{ i2c, sensorAddress };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };
}

TEST_F(SccbBusAccessI2cTest, the_sensor_is_addressed_with_the_given_7_bit_address)
{
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::continueSession, std::vector<uint8_t>{ 0x0a }));
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::stop, std::vector<uint8_t>{ 0x55 }));
    uint8_t value{ 0x55 };

    infra::VerifyingFunction<void()> onDone;
    bus.WriteRegister(0x0a, infra::MakeByteRange(value), onDone);
}

TEST_F(SccbBusAccessI2cTest, a_write_sends_the_register_with_continueSession_and_then_the_value_with_stop)
{
    testing::InSequence seq;
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::continueSession, std::vector<uint8_t>{ 0x12 }));
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::stop, std::vector<uint8_t>{ 0xab }));
    uint8_t value{ 0xab };

    infra::VerifyingFunction<void()> onDone;
    bus.WriteRegister(0x12, infra::MakeByteRange(value), onDone);
}

TEST_F(SccbBusAccessI2cTest, a_read_sends_the_register_with_stop_before_receiving)
{
    testing::InSequence seq;
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::stop, std::vector<uint8_t>{ 0x3c }));
    EXPECT_CALL(i2c, ReceiveDataMock(sensorAddress, hal::Action::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0xef }));
    uint8_t result{ 0x00 };

    infra::VerifyingFunction<void()> onDone;
    bus.ReadRegister(0x3c, infra::MakeByteRange(result), onDone);

    EXPECT_EQ(0xef, result);
}

TEST_F(SccbBusAccessI2cTest, a_read_never_uses_a_repeated_start)
{
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, testing::Ne(hal::Action::repeatedStart),
                         std::vector<uint8_t>{ 0x10 }));
    EXPECT_CALL(i2c, ReceiveDataMock(sensorAddress, hal::Action::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0x00 }));
    uint8_t result{ 0x00 };

    infra::VerifyingFunction<void()> onDone;
    bus.ReadRegister(0x10, infra::MakeByteRange(result), onDone);
}

TEST_F(SccbBusAccessI2cSlowBusTest, a_write_completes_only_after_the_value_has_been_sent)
{
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::continueSession, std::vector<uint8_t>{ 0x05 }));
    uint8_t value{ 0xcd };
    bus.WriteRegister(0x05, infra::MakeByteRange(value), [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::stop, std::vector<uint8_t>{ 0xcd }));
    i2c.onSent(hal::Result::complete, 1);

    EXPECT_CALL(done, callback());
    i2c.onSent(hal::Result::complete, 1);
}

TEST_F(SccbBusAccessI2cSlowBusTest, a_read_completes_only_after_the_value_has_been_received)
{
    EXPECT_CALL(i2c, SendDataMock(sensorAddress, hal::Action::stop, std::vector<uint8_t>{ 0x07 }));
    uint8_t result{ 0x00 };
    bus.ReadRegister(0x07, infra::MakeByteRange(result), [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, ReceiveDataMock(sensorAddress, hal::Action::stop))
        .WillOnce(testing::Return(std::vector<uint8_t>{ 0x42 }));
    i2c.onSent(hal::Result::complete, 1);

    EXPECT_CALL(done, callback());
    i2c.onReceived(hal::Result::complete);

    EXPECT_EQ(0x42, result);
}
