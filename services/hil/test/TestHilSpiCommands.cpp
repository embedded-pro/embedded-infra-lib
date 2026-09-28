#include "hal/interfaces/test_doubles/SpiMock.hpp"
#include "hal/synchronous_interfaces/test_doubles/SynchronousSpiMock.hpp"
#include "services/hil/commands/HilSpiCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 2> spiKeys{ { "clk", "sync" } };

    class SpiFactoryStub
        : public services::HilSpiFactory
    {
    public:
        explicit SpiFactoryStub(const services::HilPinNaming& naming)
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

        services::HilStatus Prepare(uint8_t, const services::HilArguments& arguments) override
        {
            services::HilStatus status = services::HilStatus::done;
            arguments.Pin("clk", naming, clock, status);
            arguments.Flag("sync", synchronous, status);

            if (status == services::HilStatus::done && !clock)
                return services::HilStatus::usage;

            return status;
        }

        services::HilStatus Open(uint8_t index, const services::HilArguments&, services::HilPinOwner& pins, services::HilSpiHandle& handle) override
        {
            hal::GpioPin* pin = nullptr;
            services::HilStatus status = pins.ClaimFunction(*clock, 1, index, pin);
            if (status != services::HilStatus::done)
                return status;

            if (synchronous)
                handle.synchronous = &synchronousSpi;
            else if (asynchronous)
                handle.spi = &manualSpi;
            else
                handle.spi = &spi;

            return services::HilStatus::done;
        }

        void Close(uint8_t, const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        const services::HilPinNaming& naming;
        std::optional<services::HilPinId> clock;
        bool synchronous = false;
        bool asynchronous = false;
        testing::StrictMock<hal::SpiMock> spi;
        testing::StrictMock<hal::SpiAsynchronousMock> manualSpi;
        testing::StrictMock<hal::SynchronousSpiMock> synchronousSpi;
    };
}

class HilSpiCommandsTest
    : public services::HilFixture
{
public:
    SpiFactoryStub factory{ naming };
    services::HilSpiCommands::WithCapacity<4> spi{ context, factory };
};

TEST_F(HilSpiCommandsTest, open_and_close)
{
    Execute("spi.open 1 clk=PB4");
    EXPECT_EQ((services::HilPinId{ 1, 4 }), pinFactory.constructed[0]);
    Execute("spi.open 1 clk=PB5");
    Execute("spi.close 1");
    Execute("spi.close 1");

    EXPECT_EQ("OK\r\nERR busy\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(pinFactory.constructed[0].has_value());
}

TEST_F(HilSpiCommandsTest, open_reports_errors)
{
    Execute("spi.open 2 clk=PB4");
    Execute("spi.open 1");
    Execute("spi.open 1 clk=PF7");
    Execute("spi.open 1 mosi=PB4");

    EXPECT_EQ("ERR range\r\nERR usage\r\nERR pin\r\nERR usage\r\n", Output());
}

TEST_F(HilSpiCommandsTest, transfer_pads_and_reports_received_bytes)
{
    Execute("spi.open 0 clk=PB4");

    EXPECT_CALL(factory.spi, SendDataMock(std::vector<uint8_t>{ 0x01, 0x02, 0x00 }, hal::SpiAction::continueSession));
    EXPECT_CALL(factory.spi, ReceiveDataMock(hal::SpiAction::continueSession)).WillOnce(testing::Return(std::vector<uint8_t>{ 0xaa, 0xbb, 0xcc }));
    Execute("spi.xfer 0 0102 rx=3 continue=1");
    Execute("spi.xfer 0 00");
    ExecuteAllActions();

    EXPECT_EQ("OK\r\nERR busy\r\n\r\nOK rx=aabbcc\r\n", Output());
}

TEST_F(HilSpiCommandsTest, transfer_without_receive)
{
    Execute("spi.open 0 clk=PB4");

    EXPECT_CALL(factory.spi, SendDataMock(std::vector<uint8_t>{ 0x01, 0x02 }, hal::SpiAction::stop));
    Execute("spi.xfer 0 0102 rx=0");
    ExecuteAllActions();

    EXPECT_EQ("OK\r\n\r\nOK rx=\r\n", Output());
}

TEST_F(HilSpiCommandsTest, transfer_checks_its_arguments)
{
    Execute("spi.open 0 clk=PB4");

    Execute("spi.xfer 1 00");
    Execute("spi.xfer 0 -");
    Execute("spi.xfer 0 00 rx=5");
    Execute("spi.xfer 0 0011223344");

    EXPECT_EQ("OK\r\nERR notopen\r\nERR usage\r\nERR range\r\nERR range\r\n", Output());
}

TEST_F(HilSpiCommandsTest, transfer_times_out)
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

TEST_F(HilSpiCommandsTest, synchronous_transfer_answers_immediately)
{
    Execute("spi.open 0 clk=PB4 sync=1");

    EXPECT_CALL(factory.synchronousSpi, ReceiveDataMock(hal::SynchronousSpi::stop)).WillOnce(testing::Return(std::vector<uint8_t>{ 0x12, 0x34 }));
    Execute("spi.xfer 0 - rx=2");

    EXPECT_EQ("OK\r\nOK rx=1234\r\n", Output());
}
