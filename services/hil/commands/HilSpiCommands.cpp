#include "services/hil/commands/HilSpiCommands.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    namespace
    {
        constexpr infra::Duration transferTimeout = std::chrono::milliseconds(1000);
    }

    HilSpiCommands::HilSpiCommands(infra::ByteRange buffers, HilContext& context, HilSpiFactory& factory)
        : services::TerminalCommands(context.terminal)
        , transmitBuffer(infra::Head(buffers, buffers.size() / 2))
        , receiveBuffer(infra::DiscardHead(buffers, buffers.size() / 2))
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::spi)
        , commands{ {
              HilBind<HilSpiCommands, &HilSpiCommands::Open>("spi.open", "<index> [key=value]...", *this, context.response),
              HilBind<HilSpiCommands, &HilSpiCommands::Transfer>("spi.xfer", "<index> <txHex|-> [rx=] [continue=]", *this, context.response),
              HilBind<HilSpiCommands, &HilSpiCommands::Close>("spi.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilSpiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilSpiCommands::Open(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return HilStatus::usage;

        uint8_t index = 0;
        HilStatus status = instance.Parse(arguments, index);
        if (status == HilStatus::done)
            status = factory.Prepare(index, arguments);
        if (status != HilStatus::done)
            return status;

        if (instance.Occupied())
            return HilStatus::busy;

        return OpenInstance(index, arguments);
    }

    HilStatus HilSpiCommands::Transfer(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "rx", "continue" }))
            return HilStatus::usage;

        std::size_t transmitSize = 0;
        bool continueSession = false;
        HilStatus status = instance.Find(arguments);
        if (status == HilStatus::done)
            status = HilArguments::ParseHex(arguments.Positional(1), transmitBuffer, transmitSize);
        auto receiveSize = static_cast<uint32_t>(transmitSize);
        arguments.Number("rx", receiveSize, 0, static_cast<uint32_t>(receiveBuffer.size()), status);
        arguments.Flag("continue", continueSession, status);
        if (status != HilStatus::done)
            return status;

        if (transferring)
            return HilStatus::busy;

        const auto length = std::max<std::size_t>(transmitSize, receiveSize);
        if (length == 0)
            return HilStatus::usage;

        StartTransfer(transmitSize, receiveSize, length, continueSession);
        return HilStatus::done;
    }

    HilStatus HilSpiCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle = HilSpiHandle{};
        ++generation;
        transferring = false;
        awaiting = false;
        timer.Cancel();
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    HilStatus HilSpiCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilSpiHandle opened;
        HilStatus status = factory.Open(index, arguments, pins, opened);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened.spi != nullptr || opened.synchronous != nullptr);

        handle = opened;
        instance.Open(index);
        context.response.Ok();
        return HilStatus::done;
    }

    void HilSpiCommands::StartTransfer(std::size_t transmitSize, std::size_t receiveSize, std::size_t length, bool continueSession)
    {
        std::fill(transmitBuffer.begin() + transmitSize, transmitBuffer.begin() + length, 0);
        std::ranges::fill(receiveBuffer, 0);

        auto send = transmitSize != 0 ? infra::ConstByteRange(infra::Head(transmitBuffer, length)) : infra::ConstByteRange();
        auto receive = receiveSize != 0 ? infra::Head(receiveBuffer, length) : infra::ByteRange();
        reportSize = receiveSize;

        if (handle.synchronous != nullptr)
        {
            handle.synchronous->SendAndReceive(send, receive, continueSession ? hal::SynchronousSpi::continueSession : hal::SynchronousSpi::stop);
            Report();
        }
        else
            TransferAsynchronous(send, receive, continueSession ? hal::SpiAction::continueSession : hal::SpiAction::stop);
    }

    void HilSpiCommands::TransferAsynchronous(infra::ConstByteRange send, infra::ByteRange receive, hal::SpiAction nextAction)
    {
        transferring = true;
        awaiting = true;
        const auto current = ++generation;
        timer.Start(transferTimeout, [this]()
            {
                Timeout();
            });

        handle.spi->SendAndReceive(send, receive, nextAction, [this, current]()
            {
                Done(current);
            });
    }

    void HilSpiCommands::Done(uint32_t current)
    {
        if (current != generation)
            return;

        transferring = false;

        if (awaiting)
        {
            awaiting = false;
            timer.Cancel();
            Report();
        }
    }

    void HilSpiCommands::Timeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(HilStatus::timeout);
    }

    void HilSpiCommands::Report()
    {
        (context.response.Ok() << " rx=").Hex(infra::Head(infra::ConstByteRange(receiveBuffer), reportSize));
    }

    void HilSpiCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
