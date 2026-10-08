#include "drivers/audio/cs43l22/Cs43l22BusAccessI2c.hpp"
#include "hal/interfaces/I2c.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace
{
    constexpr hal::I2cAddress deviceAddress{ 0x4a };
    constexpr hal::I2cAddress deviceAddressAd0High{ 0x4b };
    constexpr std::size_t maxBytes = 3;

    struct Bytes
    {
        std::array<uint8_t, maxBytes> values{};
        std::size_t size{ 0 };

        bool operator==(const Bytes& other) const = default;
    };

    template<class... Values>
    Bytes MakeBytes(Values... values)
    {
        return Bytes{ { static_cast<uint8_t>(values)... }, sizeof...(Values) };
    }

    class I2cMasterBytesMock
        : public hal::I2cMaster
    {
    public:
        void SendData(hal::I2cAddress address, infra::ConstByteRange data, hal::Action nextAction, infra::Function<void(hal::Result, uint32_t numberOfBytesSent)> sent) override
        {
            ASSERT_LE(data.size(), maxBytes);

            Bytes bytes;
            bytes.size = data.size();
            std::copy(data.begin(), data.end(), bytes.values.data());
            SendDataMock(address, nextAction, bytes);

            if (completeAutomatically)
                sent(hal::Result::complete, static_cast<uint32_t>(data.size()));
            else
                onSent = sent;
        }

        void ReceiveData(hal::I2cAddress address, infra::ByteRange data, hal::Action nextAction, infra::Function<void(hal::Result)> received) override
        {
            const Bytes bytes = ReceiveDataMock(address, nextAction);
            ASSERT_EQ(data.size(), bytes.size);
            std::copy(bytes.values.data(), bytes.values.data() + bytes.size, data.begin());

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

    class Cs43l22BusAccessI2cTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        testing::StrictMock<I2cMasterBytesMock> i2c;
        drivers::Cs43l22BusAccessI2c bus{ i2c };
    };

    class Cs43l22BusAccessI2cSlowBusTest
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        Cs43l22BusAccessI2cSlowBusTest()
        {
            i2c.completeAutomatically = false;
        }

        testing::StrictMock<I2cMasterBytesMock> i2c;
        drivers::Cs43l22BusAccessI2c bus{ i2c };
        testing::StrictMock<infra::MockCallback<void()>> done;
    };
}

TEST_F(Cs43l22BusAccessI2cTest, the_device_is_addressed_with_its_7_bit_address_when_ad0_is_low)
{
    EXPECT_TRUE(drivers::Cs43l22BusAccessI2c::addressAd0Low == deviceAddress);
}

TEST_F(Cs43l22BusAccessI2cTest, the_device_is_addressed_with_its_7_bit_address_when_ad0_is_high)
{
    EXPECT_TRUE(drivers::Cs43l22BusAccessI2c::addressAd0High == deviceAddressAd0High);
}

TEST_F(Cs43l22BusAccessI2cTest, a_write_sends_the_register_and_then_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, MakeBytes(0x02)));
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, MakeBytes(0x9e)));
    std::array<uint8_t, 1> data{ 0x9e };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x02, data, done);
}

TEST_F(Cs43l22BusAccessI2cTest, a_read_sends_the_register_and_then_receives_the_data)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, MakeBytes(0x01)));
    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(MakeBytes(0xe3)));
    std::array<uint8_t, 1> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x01, data, done);

    EXPECT_EQ(0xe3, data[0]);
}

TEST_F(Cs43l22BusAccessI2cTest, a_write_of_several_bytes_sets_the_auto_increment_bit_in_the_register)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, MakeBytes(0xa0)));
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, MakeBytes(0x11, 0x22)));
    std::array<uint8_t, 2> data{ 0x11, 0x22 };

    infra::VerifyingFunction<void()> done;
    bus.WriteRegister(0x20, data, done);
}

TEST_F(Cs43l22BusAccessI2cTest, a_read_of_several_bytes_sets_the_auto_increment_bit_in_the_register)
{
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, MakeBytes(0xa0)));
    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(MakeBytes(0x33, 0x44)));
    std::array<uint8_t, 2> data{};

    infra::VerifyingFunction<void()> done;
    bus.ReadRegister(0x20, data, done);

    EXPECT_EQ((std::array<uint8_t, 2>{ 0x33, 0x44 }), data);
}

TEST_F(Cs43l22BusAccessI2cTest, the_other_address_is_used_when_given)
{
    drivers::Cs43l22BusAccessI2c busAd0High{ i2c, drivers::Cs43l22BusAccessI2c::addressAd0High };
    testing::InSequence sequence;
    EXPECT_CALL(i2c, SendDataMock(deviceAddressAd0High, hal::Action::continueSession, MakeBytes(0x04)));
    EXPECT_CALL(i2c, SendDataMock(deviceAddressAd0High, hal::Action::stop, MakeBytes(0xaf)));
    std::array<uint8_t, 1> data{ 0xaf };

    infra::VerifyingFunction<void()> done;
    busAd0High.WriteRegister(0x04, data, done);
}

TEST_F(Cs43l22BusAccessI2cSlowBusTest, a_write_completes_only_after_the_data_has_been_sent)
{
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::continueSession, MakeBytes(0x02)));
    std::array<uint8_t, 1> data{ 0x9e };
    bus.WriteRegister(0x02, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::stop, MakeBytes(0x9e)));
    auto registerSent = i2c.onSent;
    registerSent(hal::Result::complete, 1);

    EXPECT_CALL(done, callback());
    auto dataSent = i2c.onSent;
    dataSent(hal::Result::complete, 1);
}

TEST_F(Cs43l22BusAccessI2cSlowBusTest, a_read_completes_only_after_the_data_has_been_received)
{
    EXPECT_CALL(i2c, SendDataMock(deviceAddress, hal::Action::repeatedStart, MakeBytes(0x01)));
    std::array<uint8_t, 1> data{};
    bus.ReadRegister(0x01, data, [this]()
        {
            done.callback();
        });

    EXPECT_CALL(i2c, ReceiveDataMock(deviceAddress, hal::Action::stop)).WillOnce(testing::Return(MakeBytes(0xe3)));
    auto registerSent = i2c.onSent;
    registerSent(hal::Result::complete, 1);

    EXPECT_CALL(done, callback());
    auto received = i2c.onReceived;
    received(hal::Result::complete);
    EXPECT_EQ(0xe3, data[0]);
}
