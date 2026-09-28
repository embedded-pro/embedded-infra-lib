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
        : HilSingleInstanceGroup(context, factory, HilOwners::spi)
        , transmitBuffer(infra::Head(buffers, buffers.size() / 2))
        , receiveBuffer(infra::DiscardHead(buffers, buffers.size() / 2))
        , factory(factory)
        , transfer(Context().response)
        , commands{ {
              OpenCommand("spi.open", "<index> [key=value]..."),
              HilBind<HilSpiCommands, &HilSpiCommands::Transfer>("spi.xfer", "<index> <txHex|-> [rx=] [continue=]", *this, Context().response),
              CloseCommand("spi.close", "<index>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilSpiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilSpiCommands::Transfer(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "rx", "continue" }))
            return HilStatus::usage;

        std::size_t transmitSize = 0;
        bool continueSession = false;
        HilStatus status = Instance().Find(arguments);
        if (status == HilStatus::done)
            status = HilArguments::ParseHex(arguments.Positional(1), transmitBuffer, transmitSize);
        auto receiveSize = static_cast<uint32_t>(transmitSize);
        arguments.Number("rx", receiveSize, 0, static_cast<uint32_t>(receiveBuffer.size()), status);
        arguments.Flag("continue", continueSession, status);
        if (status != HilStatus::done)
            return status;

        if (transfer.Busy())
            return HilStatus::busy;

        const auto length = std::max<std::size_t>(transmitSize, receiveSize);
        if (length == 0)
            return HilStatus::usage;

        StartTransfer(transmitSize, receiveSize, length, continueSession);
        return HilStatus::done;
    }

    HilStatus HilSpiCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilSpiHandle opened;
        HilStatus status = factory.Open(index, arguments, Pins(), opened);
        if (status != HilStatus::done)
            return status;

        really_assert(opened.spi != nullptr || opened.synchronous != nullptr);

        handle = opened;
        return HilStatus::done;
    }

    void HilSpiCommands::CloseInstance()
    {
        handle = HilSpiHandle{};
        transfer.Cancel();
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
        const auto operation = transfer.Start(transferTimeout);

        handle.spi->SendAndReceive(send, receive, nextAction, [this, operation]()
            {
                if (transfer.Complete(operation))
                    Report();
            });
    }

    void HilSpiCommands::Report() const
    {
        (Context().response.Ok() << " rx=").Hex(infra::Head(infra::ConstByteRange(receiveBuffer), reportSize));
    }
}
