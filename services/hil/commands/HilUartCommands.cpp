#include "services/hil/commands/HilUartCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        constexpr uint32_t defaultReceiveTimeoutMs = 1000;
        constexpr uint32_t maximumReceiveTimeoutMs = 10000;
        constexpr uint32_t sendMarginMs = 1000;
        constexpr uint32_t bitsPerFrame = 12;
    }

    HilUartCommands::HilUartCommands(infra::MemoryRange<uint8_t> receiveStorage, infra::ByteRange transmitBuffer, HilContext& context, HilUartFactory& factory)
        : HilSingleInstanceGroup(context, factory, HilOwners::uart)
        , transmitBuffer(transmitBuffer)
        , factory(factory)
        , received(receiveStorage, [this]()
              {
                  CheckReceive();
              })
        , sending(context.response)
        , commands{ {
              OpenCommand("uart.open", "<index> [key=value]..."),
              HilBind<HilUartCommands, &HilUartCommands::Send>("uart.send", "<index> <hex>", *this, context.response),
              HilBind<HilUartCommands, &HilUartCommands::Receive>("uart.recv", "<index> [timeout=] [len=]", *this, context.response),
              CloseCommand("uart.close", "<index>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilUartCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilUartCommands::Send(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        if (sending.Busy())
            return HilStatus::busy;

        std::size_t size = 0;
        status = HilArguments::ParseHex(arguments.Positional(1), transmitBuffer, size);
        if (status != HilStatus::done)
            return status;

        if (size == 0)
            return HilStatus::usage;

        auto data = infra::Head(infra::ConstByteRange(transmitBuffer), size);

        if (handle.synchronous != nullptr)
        {
            handle.synchronous->SendData(data);
            context.response.Ok();
        }
        else
            SendAsynchronous(data);

        return HilStatus::done;
    }

    HilStatus HilUartCommands::Receive(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "timeout", "len" }))
            return HilStatus::usage;

        uint32_t timeout = defaultReceiveTimeoutMs;
        uint32_t length = 0;
        HilStatus status = instance.Find(arguments);
        arguments.Number("timeout", timeout, 0, maximumReceiveTimeoutMs, status);
        arguments.Number("len", length, 1, static_cast<uint32_t>(received.EmptySize()), status);
        if (status != HilStatus::done)
            return status;

        if (receiveWanted.has_value())
            return HilStatus::busy;

        StartReceive(arguments.Has("len") ? std::make_optional<std::size_t>(length) : std::nullopt, std::chrono::milliseconds(timeout));
        return HilStatus::done;
    }

    HilStatus HilUartCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilUartHandle opened;
        HilStatus status = factory.Open(index, arguments, pins, timeKeeper, opened);
        if (status != HilStatus::done)
            return status;

        really_assert(opened.synchronous != nullptr || (opened.serial != nullptr && opened.baudRate != 0));

        received.Consume(received.Size());
        handle = opened;

        if (handle.serial != nullptr)
            handle.serial->ReceiveData([this](infra::ConstByteRange data)
                {
                    Received(data);
                });

        return HilStatus::done;
    }

    void HilUartCommands::CloseInstance()
    {
        handle = HilUartHandle{};
        sending.Cancel();
        receiveWanted = std::nullopt;
        receiveTimer.Cancel();
    }

    void HilUartCommands::SendAsynchronous(infra::ConstByteRange data)
    {
        const auto operation = sending.Start(std::chrono::milliseconds(sendMarginMs + data.size() * bitsPerFrame * 1000 / handle.baudRate));

        handle.serial->SendData(data, [this, operation]()
            {
                if (sending.Complete(operation))
                    context.response.Ok();
            });
    }

    void HilUartCommands::StartReceive(std::optional<std::size_t> length, infra::Duration timeout)
    {
        if (handle.synchronous != nullptr)
        {
            timeKeeper.Arm(length.has_value() ? timeout : infra::Duration());
            DrainSynchronous(length.value_or(0));
            FinishReceive();
        }
        else if (!length || received.Size() >= *length)
            FinishReceive();
        else
        {
            receiveWanted = length;
            receiveTimer.Start(timeout, [this]()
                {
                    FinishReceive();
                });
        }
    }

    void HilUartCommands::Received(infra::ConstByteRange data)
    {
        for (auto byte : data)
            if (!received.Full())
                received.AddFromInterrupt(byte);
    }

    void HilUartCommands::DrainSynchronous(std::size_t wanted)
    {
        uint8_t byte = 0;

        while (!received.Full())
        {
            if (received.Size() >= wanted)
                timeKeeper.Arm(infra::Duration());

            if (!handle.synchronous->ReceiveData(infra::MakeByteRange(byte)))
                break;

            received.AddFromInterrupt(byte);
        }
    }

    void HilUartCommands::CheckReceive()
    {
        if (receiveWanted.has_value() && received.Size() >= *receiveWanted)
        {
            receiveTimer.Cancel();
            FinishReceive();
        }
    }

    void HilUartCommands::FinishReceive()
    {
        receiveWanted = std::nullopt;
        auto line = context.response.Ok();
        line << " data=";

        while (!received.Empty())
        {
            auto chunk = received.ContiguousRange();
            line.Hex(chunk);
            received.Consume(chunk.size());
        }
    }
}
