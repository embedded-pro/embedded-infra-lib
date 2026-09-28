#include "services/hil/commands/SpiCommands.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services::hil
{
    namespace
    {
        constexpr infra::Duration transferTimeout = std::chrono::milliseconds(1000);
    }

    SpiCommands::SpiCommands(infra::ByteRange buffers, Context& context, SpiFactory& factory)
        : services::TerminalCommands(context.terminal)
        , transmitBuffer(infra::Head(buffers, buffers.size() / 2))
        , receiveBuffer(infra::DiscardHead(buffers, buffers.size() / 2))
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::spi)
        , commands{ {
              Bind<SpiCommands, &SpiCommands::Open>("spi.open", "<index> [key=value]...", *this, context.response),
              Bind<SpiCommands, &SpiCommands::Transfer>("spi.xfer", "<index> <txHex|-> [rx=] [continue=]", *this, context.response),
              Bind<SpiCommands, &SpiCommands::Close>("spi.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> SpiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status SpiCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return Status::usage;

        uint8_t index = 0;
        Status status = instance.Parse(arguments, index);
        if (status == Status::done)
            status = factory.Prepare(index, arguments);
        if (status != Status::done)
            return status;

        if (instance.Occupied())
            return Status::busy;

        return OpenInstance(index, arguments);
    }

    Status SpiCommands::Transfer(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "rx", "continue" }))
            return Status::usage;

        std::size_t transmitSize = 0;
        bool continueSession = false;
        Status status = instance.Find(arguments);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(1), transmitBuffer, transmitSize);
        auto receiveSize = static_cast<uint32_t>(transmitSize);
        arguments.Number("rx", receiveSize, 0, static_cast<uint32_t>(receiveBuffer.size()), status);
        arguments.Flag("continue", continueSession, status);
        if (status != Status::done)
            return status;

        if (transferring)
            return Status::busy;

        const auto length = std::max<std::size_t>(transmitSize, receiveSize);
        if (length == 0)
            return Status::usage;

        StartTransfer(transmitSize, receiveSize, length, continueSession);
        return Status::done;
    }

    Status SpiCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        handle = SpiHandle{};
        ++generation;
        transferring = false;
        awaiting = false;
        timer.Cancel();
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return Status::done;
    }

    Status SpiCommands::OpenInstance(uint8_t index, const Arguments& arguments)
    {
        SpiHandle opened;
        Status status = factory.Open(index, arguments, pins, opened);
        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened.spi != nullptr || opened.synchronous != nullptr);

        handle = opened;
        instance.Open(index);
        context.response.Ok();
        return Status::done;
    }

    void SpiCommands::StartTransfer(std::size_t transmitSize, std::size_t receiveSize, std::size_t length, bool continueSession)
    {
        std::fill(transmitBuffer.begin() + transmitSize, transmitBuffer.begin() + length, 0);
        std::fill(receiveBuffer.begin(), receiveBuffer.end(), 0);

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

    void SpiCommands::TransferAsynchronous(infra::ConstByteRange send, infra::ByteRange receive, hal::SpiAction nextAction)
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

    void SpiCommands::Done(uint32_t current)
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

    void SpiCommands::Timeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }

    void SpiCommands::Report()
    {
        (context.response.Ok() << " rx=").Hex(infra::Head(infra::ConstByteRange(receiveBuffer), reportSize));
    }

    void SpiCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
