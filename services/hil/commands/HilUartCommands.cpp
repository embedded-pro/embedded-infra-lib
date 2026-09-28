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
        : services::TerminalCommands(context.terminal)
        , transmitBuffer(transmitBuffer)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::uart)
        , received(receiveStorage, [this]()
              {
                  CheckReceive();
              })
        , commands{ {
              HilBind<HilUartCommands, &HilUartCommands::Open>("uart.open", "<index> [key=value]...", *this, context.response),
              HilBind<HilUartCommands, &HilUartCommands::Send>("uart.send", "<index> <hex>", *this, context.response),
              HilBind<HilUartCommands, &HilUartCommands::Receive>("uart.recv", "<index> [timeout=] [len=]", *this, context.response),
              HilBind<HilUartCommands, &HilUartCommands::Close>("uart.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilUartCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilUartCommands::Open(const HilArguments& arguments)
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

    HilStatus HilUartCommands::Send(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        if (transmitting)
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

    HilStatus HilUartCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle = HilUartHandle{};
        ++sendGeneration;
        transmitting = false;
        awaitingSend = false;
        receiveWanted = std::nullopt;
        sendTimer.Cancel();
        receiveTimer.Cancel();
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    HilStatus HilUartCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilUartHandle opened;
        HilStatus status = factory.Open(index, arguments, pins, timeKeeper, opened);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened.synchronous != nullptr || (opened.serial != nullptr && opened.baudRate != 0));

        while (!received.Empty())
            received.Get();

        handle = opened;
        instance.Open(index);

        if (handle.serial != nullptr)
            handle.serial->ReceiveData([this](infra::ConstByteRange data)
                {
                    Received(data);
                });

        context.response.Ok();
        return HilStatus::done;
    }

    void HilUartCommands::SendAsynchronous(infra::ConstByteRange data)
    {
        transmitting = true;
        awaitingSend = true;
        const auto generation = ++sendGeneration;
        const auto timeout = std::chrono::milliseconds(sendMarginMs + data.size() * bitsPerFrame * 1000 / handle.baudRate);
        sendTimer.Start(timeout, [this]()
            {
                SendTimeout();
            });

        handle.serial->SendData(data, [this, generation]()
            {
                SendDone(generation);
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
            auto byte = received.Get();
            line.Hex(infra::MakeByteRange(byte));
        }
    }

    void HilUartCommands::SendDone(uint32_t generation)
    {
        if (generation != sendGeneration)
            return;

        transmitting = false;

        if (awaitingSend)
        {
            awaitingSend = false;
            sendTimer.Cancel();
            context.response.Ok();
        }
    }

    void HilUartCommands::SendTimeout()
    {
        if (!awaitingSend)
            return;

        awaitingSend = false;
        context.response.Error(HilStatus::timeout);
    }

    void HilUartCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
