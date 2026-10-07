#include "drivers/audio/wm8994/Wm8994BusAccessI2c.hpp"
#include "hal/interfaces/I2c.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr hal::I2cAddress deviceAddress{ 0x1a };

    using Bytes = std::array<uint8_t, 2>;

    class I2cMasterBytesMock
        : public hal::I2cMaster
    {
    public:
        void SendData(hal::I2cAddress address, infra::ConstByteRange data, hal::Action nextAction, infra::Function<void(hal::Result, uint32_t numberOfBytesSent)> sent) override
        {
            ASSERT_EQ(std::size_t(2), data.size());

            SendDataMock(address, nextAction, Bytes{ data[0], data[1] });

            if (completeAutomatically)
                sent(hal::Result::complete, static_cast<uint32_t>(data.size()));
            else
                onSent = sent;
        }

        void ReceiveData(hal::I2cAddress address, infra::ByteRange data, hal::Action nextAction, infra::Function<void(hal::Result)> received) override
        {
            ASSERT_EQ(std::size_t(2), data.size());

            const Bytes bytes = ReceiveDataMock(address, nextAction);
            data[0] = bytes[0];
            data[1] = bytes[1];

            if (completeAutomatically)
                received(hal::Result::complete);
            else
                onReceived = received;
        }

        MOCK_METHOD(void, SendDataMock, (hal::I2cAddress address, hal::Action nextAction, Bytes data));
        MOCK_METHOD(Bytes, ReceiveDataMock, (hal::I2cAddress address, hal::Action nextAction));

        bool completeAutomatically = true;
        infra::Function<void(hal::Result, uint32_t numberOfBytesSent)> onSent;
        infra::Function<void(hal::Result)> onReceived;
    };

    class Wm8994BusAccessI2cTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<I2cMasterBytesMock> i2c;
        drivers::Wm8994BusAccessI2c bus{ i2c };
    };

    class Wm8994BusAccessI2cSlowBusTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        Wm8994BusAccessI2cSlowBusTest()
        {
            i2c.completeAutomatically = false;
        }

        testing::StrictMock<I2cMasterBytesMock> i2c;
        drivers::Wm8994BusAccessI2c bus{ i2c };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };
}

TEST_F(Wm8994BusAccessI2cTest, the_device_is_addressed_with_its_7_bit_address)
{
    EXPECT_TRUE(drivers::Wm8994BusAccessI2c::deviceAddress == deviceAddress);
}

TEST_F(Wm8994BusAccessI2cTest, a_write_sends_the_most_significant_byte_of_the_register_first_and_then_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, Bytes{ 0x01, 0x02 }));
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, Bytes{ 0x03, 0x04 }));
    Bytes data{ 0x03, 0x04 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x0102, data, done);
}

TEST_F(Wm8994BusAccessI2cTest, a_register_above_0xff_keeps_both_bytes)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, Bytes{ 0x02, 0x10 }));
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, Bytes{ 0x00, 0x83 }));
    Bytes data{ 0x00, 0x83 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x0210, data, done);
}

TEST_F(Wm8994BusAccessI2cTest, a_read_sends_the_most_significant_byte_of_the_register_first_and_then_receives_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, Bytes{ 0x01, 0x02 }));
    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 0x03, 0x04 }));
    Bytes data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x0102, data, done);

    EXPECT_EQ((Bytes{ 0x03, 0x04 }), data);
}

TEST_F(Wm8994BusAccessI2cTest, the_chip_id_register_is_read_as_two_bytes)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, Bytes{ 0x00, 0x00 }));
    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 0x89, 0x94 }));
    Bytes data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x0000, data, done);

    EXPECT_EQ((Bytes{ 0x89, 0x94 }), data);
}

TEST_F(Wm8994BusAccessI2cSlowBusTest, a_write_completes_only_after_the_data_has_been_sent)
{
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, Bytes{ 0x01, 0x02 }));
    Bytes data{ 0x03, 0x04 };
    bus.WriteRegister(0x0102, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, Bytes{ 0x03, 0x04 }));
    auto registerSent = i2c.onSent;
    registerSent(hal::Result::complete, 2);

    EXPECT_CALL(done, callback());
    auto dataSent = i2c.onSent;
    dataSent(hal::Result::complete, 2);
}

TEST_F(Wm8994BusAccessI2cSlowBusTest, a_read_completes_only_after_the_data_has_been_received)
{
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, Bytes{ 0x01, 0x02 }));
    Bytes data{};
    bus.ReadRegister(0x0102, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(Bytes{ 0x03, 0x04 }));
    auto addressSent = i2c.onSent;
    addressSent(hal::Result::complete, 2);

    EXPECT_CALL(done, callback());
    auto received = i2c.onReceived;
    received(hal::Result::complete);
    EXPECT_EQ((Bytes{ 0x03, 0x04 }), data);
}
