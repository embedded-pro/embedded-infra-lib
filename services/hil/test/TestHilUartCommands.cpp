#include "hal/interfaces/test_doubles/SerialCommunicationMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousSerialCommunicationMock.hpp"
#include "infra/util/test_helper/MockHelpers.hpp"
#include "services/hil/commands/HilUartCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 3> uartKeys{ { "tx", "rx", "sync" } };

    class UartFactoryStub
        : public services::HilUartFactory
    {
    public:
        explicit UartFactoryStub(const services::HilPinNaming& naming)
            : naming(naming)
        {}

        uint8_t Instances() const override
        {
            return 4;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(uartKeys);
        }

        services::HilStatus Prepare(uint8_t index, const services::HilArguments& arguments) override
        {
            services::HilStatus status = services::HilStatus::done;
            arguments.Pin("tx", naming, tx, status);
            arguments.Pin("rx", naming, rx, status);
            arguments.Flag("sync", synchronous, status);

            if (status == services::HilStatus::done && index == 0)
                return services::HilStatus::busy;

            return status;
        }

        services::HilStatus Open(uint8_t index, const services::HilArguments&, services::HilPinOwner& pins, hal::TimeKeeper& timeKeeper, services::HilUartHandle& handle) override
        {
            hal::GpioPin* pin = nullptr;
            services::HilStatus status = pins.ClaimFunction(tx, 1, index, pin);
            if (status == services::HilStatus::done)
                status = pins.ClaimFunction(rx, 2, index, pin);
            if (status != services::HilStatus::done)
                return status;

            this->timeKeeper = &timeKeeper;
            if (synchronous)
                handle.synchronous = &synchronousSerial;
            else
            {
                handle.serial = &serial;
                handle.baudRate = 115200;
            }

            return services::HilStatus::done;
        }

        void Close(uint8_t index, const infra::Function<void()>& onClosed) override
        {
            closedIndex = index;

            if (deferClose)
                pendingClose = onClosed;
            else
                onClosed();
        }

        const services::HilPinNaming& naming;
        std::optional<services::HilPinId> tx;
        std::optional<services::HilPinId> rx;
        bool synchronous = false;
        hal::TimeKeeper* timeKeeper = nullptr;
        testing::StrictMock<hal::SerialCommunicationCleanMock> serial;
        testing::StrictMock<hal::SynchronousSerialCommunicationMock> synchronousSerial;
        std::optional<uint8_t> closedIndex;
        bool deferClose = false;
        infra::Function<void()> pendingClose;
    };
}

class HilUartCommandsTest
    : public services::HilFixture
{
public:
    void Open()
    {
        EXPECT_CALL(factory.serial, ReceiveData(testing::_)).WillOnce(testing::SaveArg<0>(&dataReceived));
        Execute("uart.open 1 tx=PB1 rx=PB0");
        ASSERT_EQ("OK\r\n", Output());
    }

    void Receive(std::vector<uint8_t> data)
    {
        dataReceived(infra::MakeRange(data));
        ExecuteAllActions();
    }

    UartFactoryStub factory{ naming };
    services::HilUartCommands::WithCapacity<4, 3> uart{ context, factory };
    infra::Function<void(infra::ConstByteRange data)> dataReceived;
};

TEST_F(HilUartCommandsTest, open_claims_pins_and_starts_receiving)
{
    Open();

    EXPECT_EQ((services::HilPinId{ 1, 1 }), pinFactory.constructed[0]);
    EXPECT_EQ((services::HilPinId{ 1, 0 }), pinFactory.constructed[1]);
}

TEST_F(HilUartCommandsTest, open_reports_errors)
{
    Execute("uart.open 4");
    Execute("uart.open 1 baud=9600");
    Execute("uart.open 1 tx=PZ1");
    Execute("uart.open 0");
    Execute("uart.open");

    EXPECT_EQ("ERR range\r\nERR usage\r\nERR pin\r\nERR busy\r\nERR usage\r\n", Output());
}

TEST_F(HilUartCommandsTest, failed_open_releases_claimed_pins)
{
    Execute("uart.open 1 tx=PB1 rx=PF7");

    EXPECT_EQ("ERR pin\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}

TEST_F(HilUartCommandsTest, second_open_is_busy)
{
    Open();

    Execute("uart.open 2");

    EXPECT_EQ("ERR busy\r\n", Output());
}

TEST_F(HilUartCommandsTest, send_answers_on_completion)
{
    Open();
    infra::Function<void()> onDone;

    EXPECT_CALL(factory.serial, SendData(infra::CheckByteRangeContents(std::vector<uint8_t>{ 0xa5, 0x5a }), testing::_)).WillOnce(testing::SaveArg<1>(&onDone));
    Execute("uart.send 1 a55a");
    Execute("uart.send 1 00");
    EXPECT_EQ("ERR busy\r\n", Output());

    onDone();
    EXPECT_EQ("\r\nOK\r\n", Output());
}

TEST_F(HilUartCommandsTest, send_times_out)
{
    Open();
    infra::Function<void()> onDone;

    EXPECT_CALL(factory.serial, SendData(testing::_, testing::_)).WillOnce(testing::SaveArg<1>(&onDone));
    Execute("uart.send 1 a55a");
    ForwardTime(std::chrono::milliseconds(1000));
    EXPECT_EQ("\r\nERR timeout\r\n", Output());

    onDone();
    EXPECT_EQ("", Output());
}

TEST_F(HilUartCommandsTest, send_checks_its_arguments)
{
    Open();

    Execute("uart.send 2 00");
    Execute("uart.send 1 -");
    Execute("uart.send 1 00112233");

    EXPECT_EQ("ERR notopen\r\nERR usage\r\nERR range\r\n", Output());
}

TEST_F(HilUartCommandsTest, receive_returns_what_arrived)
{
    Open();
    Receive({ 0x01, 0x02 });

    Execute("uart.recv 1");
    Execute("uart.recv 1");

    EXPECT_EQ("OK data=0102\r\nOK data=\r\n", Output());
}

TEST_F(HilUartCommandsTest, receive_waits_for_length)
{
    Open();
    Receive({ 0x01 });

    Execute("uart.recv 1 len=2");
    Execute("uart.recv 1");
    EXPECT_EQ("ERR busy\r\n", Output());

    Receive({ 0x02 });
    EXPECT_EQ("\r\nOK data=0102\r\n", Output());
}

TEST_F(HilUartCommandsTest, receive_times_out_with_partial_data)
{
    Open();
    Receive({ 0x01 });

    Execute("uart.recv 1 len=3 timeout=50");
    ForwardTime(std::chrono::milliseconds(50));

    EXPECT_EQ("\r\nOK data=01\r\n", Output());
}

TEST_F(HilUartCommandsTest, receive_length_is_bounded_by_capacity)
{
    Open();

    Execute("uart.recv 1 len=5");

    EXPECT_EQ("ERR range\r\n", Output());
}

TEST_F(HilUartCommandsTest, synchronous_send_and_receive)
{
    Execute("uart.open 1 sync=1");
    EXPECT_NE(nullptr, factory.timeKeeper);

    EXPECT_CALL(factory.synchronousSerial, SendDataMock(std::vector<uint8_t>{ 0x42 }));
    Execute("uart.send 1 42");

    EXPECT_CALL(factory.synchronousSerial, ReceiveDataMock())
        .WillOnce(testing::Return(hal::SynchronousSerialCommunicationMock::ReceiveDataMockResult{ true, { 0x11 } }))
        .WillOnce(testing::Return(hal::SynchronousSerialCommunicationMock::ReceiveDataMockResult{ true, { 0x22 } }))
        .WillOnce(testing::Return(hal::SynchronousSerialCommunicationMock::ReceiveDataMockResult{ false, {} }));
    Execute("uart.recv 1 len=2");

    EXPECT_EQ("OK\r\nOK\r\nOK data=1122\r\n", Output());
}

TEST_F(HilUartCommandsTest, close_releases_instance_and_pins)
{
    Open();

    Execute("uart.close 1");
    Execute("uart.send 1 00");

    EXPECT_EQ("OK\r\nERR notopen\r\n", Output());
    EXPECT_EQ(1, factory.closedIndex);
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}

TEST_F(HilUartCommandsTest, asynchronous_close_answers_when_closed)
{
    Open();
    factory.deferClose = true;

    Execute("uart.close 1");
    Execute("uart.recv 1");
    Execute("uart.open 2");
    EXPECT_EQ("ERR notopen\r\nERR busy\r\n", Output());

    factory.pendingClose();
    EXPECT_EQ("\r\nOK\r\n", Output());

    EXPECT_CALL(factory.serial, ReceiveData(testing::_));
    Execute("uart.open 2");
    EXPECT_EQ("OK\r\n", Output());
}
