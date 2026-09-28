#include "services/hil/commands/HilEthernetCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilEthernetMonitor::HilEthernetMonitor(hal::EthernetSmi& smi, hal::EthernetMac& mac, infra::ByteRange receiveStorage)
        : hal::EthernetSmiObserver(smi)
        , hal::EthernetMacObserver(mac)
        , receiveStorage(receiveStorage)
    {
        really_assert(receiveStorage.size() >= frameSize);
    }

    void HilEthernetMonitor::LinkUp(hal::LinkSpeed speed)
    {
        up = true;
        linkSpeed = speed;
    }

    void HilEthernetMonitor::LinkDown()
    {
        up = false;
    }

    infra::ByteRange HilEthernetMonitor::RequestReceiveBuffer()
    {
        auto buffer = infra::Head(infra::DiscardHead(receiveStorage, nextBuffer * frameSize), frameSize);
        nextBuffer = (nextBuffer + 1) % (receiveStorage.size() / frameSize);
        return buffer;
    }

    void HilEthernetMonitor::ReceivedFrame(uint32_t, uint32_t)
    {
        ++received;
    }

    void HilEthernetMonitor::ReceivedErrorFrame(uint32_t, uint32_t)
    {}

    void HilEthernetMonitor::SentFrame()
    {
        ++sent;
    }

    bool HilEthernetMonitor::Up() const
    {
        return up;
    }

    hal::LinkSpeed HilEthernetMonitor::Speed() const
    {
        return linkSpeed;
    }

    uint32_t HilEthernetMonitor::Received() const
    {
        return received;
    }

    uint32_t HilEthernetMonitor::Sent() const
    {
        return sent;
    }

    HilEthernetCommands::HilEthernetCommands(infra::ByteRange receiveStorage, HilContext& context, HilEthernetFactory& factory)
        : services::TerminalCommands(context.terminal)
        , receiveStorage(receiveStorage)
        , context(context)
        , factory(factory)
        , commands{ {
              HilBind<HilEthernetCommands, &HilEthernetCommands::Open>("eth.open", "[key=value]...", *this, context.response),
              HilBind<HilEthernetCommands, &HilEthernetCommands::LinkStatus>("eth.status", "", *this, context.response),
              HilBind<HilEthernetCommands, &HilEthernetCommands::Close>("eth.close", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilEthernetCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilEthernetCommands::Open(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, factory.OpenKeys()))
            return HilStatus::usage;

        HilStatus status = factory.Prepare(arguments);
        if (status != HilStatus::done)
            return status;

        if (monitor || closing)
            return HilStatus::busy;

        HilEthernetHandle handle;
        status = factory.Open(arguments, handle);
        if (status != HilStatus::done)
            return status;

        really_assert(handle.smi != nullptr && handle.mac != nullptr);

        monitor.emplace(*handle.smi, *handle.mac, receiveStorage);
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilEthernetCommands::LinkStatus(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        if (!monitor)
            return HilStatus::notOpen;

        const auto speed = monitor->Speed();
        const bool fast = speed == hal::LinkSpeed::fullDuplex100MHz || speed == hal::LinkSpeed::halfDuplex100MHz;
        const bool fullDuplex = speed == hal::LinkSpeed::fullDuplex100MHz || speed == hal::LinkSpeed::fullDuplex10MHz;

        context.response.Ok() << " link=" << (monitor->Up() ? "up" : "down") << " speed=" << (fast ? 100u : 10u) << " duplex=" << (fullDuplex ? "full" : "half")
                              << " rx=" << monitor->Received() << " tx=" << monitor->Sent();
        return HilStatus::done;
    }

    HilStatus HilEthernetCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        if (!monitor)
            return HilStatus::notOpen;

        monitor = std::nullopt;
        closing = true;
        factory.Close([this]()
            {
                closing = false;
                context.response.Ok();
            });

        return HilStatus::done;
    }
}
