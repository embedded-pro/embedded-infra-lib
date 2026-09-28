#include "hal/interfaces/test_doubles/EthernetMock.hpp"
#include "services/hil/commands/EthernetCommands.hpp"
#include "services/hil/test/HilFixture.hpp"
#include "gmock/gmock.h"

namespace
{
    constexpr std::array<const char*, 1> ethernetKeys{ { "speed" } };

    class EthernetFactoryStub
        : public services::hil::EthernetFactory
    {
    public:
        infra::MemoryRange<const char* const> OpenKeys() const override
        {
            return infra::MakeRange(ethernetKeys);
        }

        services::hil::Status Prepare(const services::hil::Arguments& arguments) override
        {
            uint32_t speed = 100;
            services::hil::Status status = services::hil::Status::done;
            arguments.Number("speed", speed, 10, 100, status);
            return status;
        }

        services::hil::Status Open(const services::hil::Arguments&, services::hil::EthernetHandle& handle) override
        {
            handle.smi = &smi;
            handle.mac = &mac;
            return services::hil::Status::done;
        }

        void Close(const infra::Function<void()>& onClosed) override
        {
            onClosed();
        }

        testing::StrictMock<hal::EthernetSmiMock> smi;
        testing::StrictMock<hal::EthernetMacMock> mac;
    };
}

class EthernetCommandsTest
    : public services::hil::HilFixture
{
public:
    EthernetFactoryStub factory;
    services::hil::EthernetCommands::WithReceiveBuffers<2> ethernet{ context, factory };
};

TEST_F(EthernetCommandsTest, status_reports_link_and_counters)
{
    Execute("eth.open");
    Execute("eth.status");

    factory.smi.GetObserver().LinkUp(hal::LinkSpeed::fullDuplex100MHz);
    factory.mac.GetObserver().ReceivedFrame(1, 64);
    factory.mac.GetObserver().ReceivedFrame(1, 64);
    factory.mac.GetObserver().ReceivedErrorFrame(1, 64);
    factory.mac.GetObserver().SentFrame();
    Execute("eth.status");

    factory.smi.GetObserver().LinkDown();
    Execute("eth.status");

    EXPECT_EQ("OK\r\nOK link=down speed=10 duplex=half rx=0 tx=0\r\nOK link=up speed=100 duplex=full rx=2 tx=1\r\nOK link=down speed=100 duplex=full rx=2 tx=1\r\n", Output());
}

TEST_F(EthernetCommandsTest, receive_buffers_rotate)
{
    Execute("eth.open");

    auto first = factory.mac.GetObserver().RequestReceiveBuffer();
    auto second = factory.mac.GetObserver().RequestReceiveBuffer();
    auto third = factory.mac.GetObserver().RequestReceiveBuffer();

    EXPECT_EQ(services::hil::EthernetMonitor::frameSize, first.size());
    EXPECT_EQ(first.begin() + services::hil::EthernetMonitor::frameSize, second.begin());
    EXPECT_EQ(first.begin(), third.begin());
}

TEST_F(EthernetCommandsTest, open_and_close_lifecycle)
{
    Execute("eth.status");
    Execute("eth.close");
    Execute("eth.open speed=1000");
    Execute("eth.open 1");
    Execute("eth.open speed=10");
    Execute("eth.open");
    Execute("eth.close");
    Execute("eth.status");

    EXPECT_EQ("ERR notopen\r\nERR notopen\r\nERR range\r\nERR usage\r\nOK\r\nERR busy\r\nOK\r\nERR notopen\r\n", Output());
    EXPECT_FALSE(factory.smi.HasObserver());
    EXPECT_FALSE(factory.mac.HasObserver());
}
