#include "hal/interfaces/test_doubles/CanMock.hpp"
#include "services/hil/commands/HilCanCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 1> canKeys{ { "loopback" } };

    class CanFactoryStub
        : public services::HilCanFactory
    {
    public:
        uint8_t Instances() const override
        {
            return 2;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(canKeys);
        }

        services::HilStatus Prepare(uint8_t, const services::HilArguments& arguments) override
        {
            bool loopback = false;
            services::HilStatus status = services::HilStatus::done;
            arguments.Flag("loopback", loopback, status);
            return status;
        }

        services::HilStatus Open(uint8_t index, const services::HilArguments&, services::HilPinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) override
        {
            hal::GpioPin* pin = nullptr;
            services::HilStatus status = pins.ClaimFunction(services::HilPinId{ 5, 0 }, 1, index, pin);
            if (status != services::HilStatus::done)
                return status;

            this->onError = onError;
            can = &this->can;
            return services::HilStatus::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            pendingClose = onClosed;
        }

        infra::Function<void(const char* error)> onError;
        infra::Function<void()> pendingClose;
        testing::StrictMock<hal::CanMock> can;
    };
}

class HilCanCommandsTest
    : public services::HilFixture
{
public:
    HilCanCommandsTest()
    {
        EXPECT_CALL(factory.can, ReceiveData(testing::_)).WillOnce(testing::SaveArg<0>(&onReceived));
        Execute("can.open 1");
        EXPECT_EQ("OK\r\n", Output());
    }

    CanFactoryStub factory;
    services::HilCanCommands can{ context, factory };
    infra::Function<void(hal::Can::Id id, const hal::Can::Message& data)> onReceived;
};

TEST_F(HilCanCommandsTest, open_reports_errors)
{
    Execute("can.open 0");
    Execute("can.open 2");
    Execute("can.open 0 loopback=2");
    Execute("can.open 0 bitrate=1");

    EXPECT_EQ("ERR busy\r\nERR range\r\nERR range\r\nERR usage\r\n", Output());
}

TEST_F(HilCanCommandsTest, send_reports_success_and_failure)
{
    infra::Function<void(bool success)> onDone;
    hal::Can::Message expected;
    expected.push_back(0x12);
    expected.push_back(0x34);

    EXPECT_CALL(factory.can, SendData(hal::Can::Id::Create11BitId(0x123), expected, testing::_)).WillOnce(testing::SaveArg<2>(&onDone));
    Execute("can.send 1 0x123 1234");
    Execute("can.send 1 0x123 1234");
    onDone(true);

    EXPECT_CALL(factory.can, SendData(hal::Can::Id::Create29BitId(0x1fffffff), hal::Can::Message{}, testing::_)).WillOnce(testing::SaveArg<2>(&onDone));
    Execute("can.send 1 0x1fffffff - ext=1");
    onDone(false);

    EXPECT_EQ("ERR busy\r\n\r\nOK\r\n\r\nERR failed\r\n", Output());
}

TEST_F(HilCanCommandsTest, send_checks_its_arguments)
{
    Execute("can.send 0 1 00");
    Execute("can.send 1 0x800 00");
    Execute("can.send 1 1 001122334455667788");

    EXPECT_EQ("ERR notopen\r\nERR range\r\nERR range\r\n", Output());
}

TEST_F(HilCanCommandsTest, send_times_out)
{
    infra::Function<void(bool success)> onDone;

    EXPECT_CALL(factory.can, SendData(testing::_, testing::_, testing::_)).WillOnce(testing::SaveArg<2>(&onDone));
    Execute("can.send 1 1 00");
    ForwardTime(std::chrono::seconds(1));
    Execute("can.send 1 1 00");
    onDone(true);

    EXPECT_EQ("\r\nERR timeout\r\nERR busy\r\n", Output());
}

TEST_F(HilCanCommandsTest, received_frames_are_events)
{
    hal::Can::Message data;
    data.push_back(0xab);

    onReceived(hal::Can::Id::Create11BitId(0x7ff), data);
    onReceived(hal::Can::Id::Create29BitId(0x12345), hal::Can::Message{});

    EXPECT_EQ("\r\nEVT can index=1 id=2047 ext=0 data=ab\r\n\r\nEVT can index=1 id=74565 ext=1 data=\r\n", Output());
}

TEST_F(HilCanCommandsTest, repeated_errors_are_throttled)
{
    factory.onError("busOff");
    factory.onError("busOff");
    factory.onError("ackError");
    ForwardTime(std::chrono::milliseconds(100));
    factory.onError("ackError");

    EXPECT_EQ("\r\nEVT can index=1 error=busOff\r\n\r\nEVT can index=1 error=ackError\r\n\r\nEVT can index=1 error=ackError\r\n", Output());
}

TEST_F(HilCanCommandsTest, close_is_asynchronous)
{
    Execute("can.close 1");
    Execute("can.send 1 1 00");
    EXPECT_EQ("ERR notopen\r\n", Output());
    EXPECT_TRUE(pinFactory.constructed[0].has_value());

    factory.pendingClose();

    EXPECT_EQ("\r\nOK\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}
