#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousSpiMock.hpp"
#include "services/hil/commands/SpiCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 2> spiKeys{ { "clk", "sync" } };

    class SpiFactoryStub
        : public services::hil::SpiFactory
    {
    public:
        explicit SpiFactoryStub(const services::hil::PinNaming& naming)
            : naming(naming)
        {}

        uint8_t Instances() const override
        {
            return 2;
        }

        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(spiKeys);
        }

        services::hil::Status Prepare(uint8_t, const services::hil::Arguments& arguments) override
        {
            services::hil::Status status = services::hil::Status::done;
            arguments.Pin("clk", naming, clock, status);
            arguments.Flag("sync", synchronous, status);

            if (status == services::hil::Status::done && !clock)
                return services::hil::Status::usage;

            return status;
        }

        services::hil::Status Open(uint8_t index, const services::hil::Arguments&, services::hil::PinOwner& pins, services::hil::SpiHandle& handle) override
        {
            hal::GpioPin* pin = nullptr;
            services::hil::Status status = pins.ClaimFunction(*clock, 1, index, pin);
            if (status != services::hil::Status::done)
                return status;

            if (synchronous)
                handle.synchronous = &synchronousSpi;
            else if (asynchronous)
                handle.spi = &manualSpi;
            else
                handle.spi = &spi;

            return services::hil::Status::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        const services::hil::PinNaming& naming;
        std::optional<services::hil::PinId> clock;
        bool synchronous = false;
        bool asynchronous = false;
        testing::StrictMock<hal::SpiMock> spi;
        testing::StrictMock<hal::SpiAsynchronousMock> manualSpi;
        testing::StrictMock<hal::SynchronousSpiMock> synchronousSpi;
    };
}

class SpiCommandsTest
    : public services::hil::HilFixture
{
public:
    SpiFactoryStub factory{ naming };
    services::hil::SpiCommands::WithCapacity<4> spi{ context, factory };
};

TEST_F(SpiCommandsTest, open_and_close)
{
    Execute("spi.open 1 clk=PB4");
    EXPECT_EQ((services::hil::PinId{ 1, 4 }), pinFactory.constructed[0]);
    Execute("spi.open 1 clk=PB5");
    Execute("spi.close 1");
    Execute("spi.close 1");

    EXPECT_EQ("OK\r\nERR busy\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}

TEST_F(SpiCommandsTest, open_reports_errors)
{
    Execute("spi.open 2 clk=PB4");
    Execute("spi.open 1");
    Execute("spi.open 1 clk=PF7");
    Execute("spi.open 1 mosi=PB4");

    EXPECT_EQ("ERR range\r\nERR usage\r\nERR pin\r\nERR usage\r\n", Output());
}

TEST_F(SpiCommandsTest, transfer_pads_and_reports_received_bytes)
{
    Execute("spi.open 0 clk=PB4");

    EXPECT_CALL(factory.spi, SendDataMock(std::vector<uint8_t>{ 0x01, 0x02, 0x00 }, hal::SpiAction::continueSession));
    EXPECT_CALL(factory.spi, ReceiveDataMock(hal::SpiAction::continueSession)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xaa, 0xbb, 0xcc }));
    Execute("spi.xfer 0 0102 rx=3 continue=1");
    Execute("spi.xfer 0 00");
    ExecuteAllActions();

    EXPECT_EQ("OK\r\nERR busy\r\n\r\nOK rx=aabbcc\r\n", Output());
}

TEST_F(SpiCommandsTest, transfer_without_receive)
{
    Execute("spi.open 0 clk=PB4");

    EXPECT_CALL(factory.spi, SendDataMock(std::vector<uint8_t>{ 0x01, 0x02 }, hal::SpiAction::stop));
    Execute("spi.xfer 0 0102 rx=0");
    ExecuteAllActions();

    EXPECT_EQ("OK\r\n\r\nOK rx=\r\n", Output());
}

TEST_F(SpiCommandsTest, transfer_checks_its_arguments)
{
    Execute("spi.open 0 clk=PB4");

    Execute("spi.xfer 1 00");
    Execute("spi.xfer 0 -");
    Execute("spi.xfer 0 00 rx=5");
    Execute("spi.xfer 0 0011223344");

    EXPECT_EQ("OK\r\nERR notopen\r\nERR usage\r\nERR range\r\nERR range\r\n", Output());
}

TEST_F(SpiCommandsTest, transfer_times_out)
{
    factory.asynchronous = true;
    Execute("spi.open 0 clk=PB4");

    EXPECT_CALL(factory.manualSpi, SendAndReceiveMock(std::vector<uint8_t>{ 0x01 }, testing::_, hal::SpiAction::stop, testing::_));
    Execute("spi.xfer 0 01");
    ForwardTime(std::chrono::seconds(1));
    EXPECT_EQ("OK\r\n\r\nERR timeout\r\n", Output());

    factory.manualSpi.onDone();
    EXPECT_EQ("", Output());
}

TEST_F(SpiCommandsTest, synchronous_transfer_answers_immediately)
{
    Execute("spi.open 0 clk=PB4 sync=1");

    EXPECT_CALL(factory.synchronousSpi, ReceiveDataMock(hal::SynchronousSpi::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x12, 0x34 }));
    Execute("spi.xfer 0 - rx=2");

    EXPECT_EQ("OK\r\nOK rx=1234\r\n", Output());
}
