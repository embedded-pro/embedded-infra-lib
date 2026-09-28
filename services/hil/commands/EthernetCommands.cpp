#include "services/hil/commands/EthernetCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    EthernetMonitor::EthernetMonitor(hal::EthernetSmi& smi, hal::EthernetMac& mac, infra::ByteRange receiveStorage)
        : hal::EthernetSmiObserver(smi)
        , hal::EthernetMacObserver(mac)
        , receiveStorage(receiveStorage)
    {
        really_assert(receiveStorage.size() >= frameSize);
    }

    void EthernetMonitor::LinkUp(hal::LinkSpeed speed)
    {
        up = true;
        linkSpeed = speed;
    }

    void EthernetMonitor::LinkDown()
    {
        up = false;
    }

    infra::ByteRange EthernetMonitor::RequestReceiveBuffer()
    {
        auto buffer = infra::Head(infra::DiscardHead(receiveStorage, nextBuffer * frameSize), frameSize);
        nextBuffer = (nextBuffer + 1) % (receiveStorage.size() / frameSize);
        return buffer;
    }

    void EthernetMonitor::ReceivedFrame(uint32_t, uint32_t)
    {
        ++received;
    }

    void EthernetMonitor::ReceivedErrorFrame(uint32_t, uint32_t)
    {}

    void EthernetMonitor::SentFrame()
    {
        ++sent;
    }

    bool EthernetMonitor::Up() const
    {
        return up;
    }

    hal::LinkSpeed EthernetMonitor::Speed() const
    {
        return linkSpeed;
    }

    uint32_t EthernetMonitor::Received() const
    {
        return received;
    }

    uint32_t EthernetMonitor::Sent() const
    {
        return sent;
    }

    EthernetCommands::EthernetCommands(infra::ByteRange receiveStorage, Context& context, EthernetFactory& factory)
        : services::TerminalCommands(context.terminal)
        , receiveStorage(receiveStorage)
        , context(context)
        , factory(factory)
        , commands{ {
              Bind<EthernetCommands, &EthernetCommands::Open>("eth.open", "[key=value]...", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::LinkStatus>("eth.status", "", *this, context.response),
              Bind<EthernetCommands, &EthernetCommands::Close>("eth.close", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> EthernetCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status EthernetCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, factory.OpenKeys()))
            return Status::usage;

        Status status = factory.Prepare(arguments);
        if (status != Status::done)
            return status;

        if (monitor || closing)
            return Status::busy;

        EthernetHandle handle;
        status = factory.Open(arguments, handle);
        if (status != Status::done)
            return status;

        really_assert(handle.smi != nullptr && handle.mac != nullptr);

        monitor.emplace(*handle.smi, *handle.mac, receiveStorage);
        context.response.Ok();
        return Status::done;
    }

    Status EthernetCommands::LinkStatus(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (!monitor)
            return Status::notOpen;

        const auto speed = monitor->Speed();
        const bool fast = speed == hal::LinkSpeed::fullDuplex100MHz || speed == hal::LinkSpeed::halfDuplex100MHz;
        const bool fullDuplex = speed == hal::LinkSpeed::fullDuplex100MHz || speed == hal::LinkSpeed::fullDuplex10MHz;

        context.response.Ok() << " link=" << (monitor->Up() ? "up" : "down") << " speed=" << (fast ? 100u : 10u) << " duplex=" << (fullDuplex ? "full" : "half")
                              << " rx=" << monitor->Received() << " tx=" << monitor->Sent();
        return Status::done;
    }

    Status EthernetCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (!monitor)
            return Status::notOpen;

        monitor = std::nullopt;
        closing = true;
        factory.Close([this]()
            {
                closing = false;
                context.response.Ok();
            });

        return Status::done;
    }
}
