#include "services/hil/commands/UartCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    namespace
    {
        constexpr uint32_t defaultReceiveTimeoutMs = 1000;
        constexpr uint32_t maximumReceiveTimeoutMs = 10000;
        constexpr uint32_t sendMarginMs = 1000;
        constexpr uint32_t bitsPerFrame = 12;
    }

    UartCommands::UartCommands(infra::MemoryRange<uint8_t> receiveStorage, infra::ByteRange transmitBuffer, Context& context, UartFactory& factory)
        : services::TerminalCommands(context.terminal)
        , transmitBuffer(transmitBuffer)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::uart)
        , received(receiveStorage, [this]()
              {
                  CheckReceive();
              })
        , commands{ {
              Bind<UartCommands, &UartCommands::Open>("uart.open", "<index> [key=value]...", *this, context.response),
              Bind<UartCommands, &UartCommands::Send>("uart.send", "<index> <hex>", *this, context.response),
              Bind<UartCommands, &UartCommands::Receive>("uart.recv", "<index> [timeout=] [len=]", *this, context.response),
              Bind<UartCommands, &UartCommands::Close>("uart.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> UartCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status UartCommands::Open(const Arguments& arguments)
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

    Status UartCommands::Send(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        if (transmitting)
            return Status::busy;

        std::size_t size = 0;
        status = ParseHex(arguments.Positional(1), transmitBuffer, size);
        if (status != Status::done)
            return status;

        if (size == 0)
            return Status::usage;

        auto data = infra::Head(infra::ConstByteRange(transmitBuffer), size);

        if (handle.synchronous != nullptr)
        {
            handle.synchronous->SendData(data);
            context.response.Ok();
        }
        else
            SendAsynchronous(data);

        return Status::done;
    }

    Status UartCommands::Receive(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "timeout", "len" }))
            return Status::usage;

        uint32_t timeout = defaultReceiveTimeoutMs;
        uint32_t length = 0;
        Status status = instance.Find(arguments);
        arguments.Number("timeout", timeout, 0, maximumReceiveTimeoutMs, status);
        arguments.Number("len", length, 1, static_cast<uint32_t>(received.EmptySize()), status);
        if (status != Status::done)
            return status;

        if (receiveWanted)
            return Status::busy;

        StartReceive(arguments.Has("len") ? std::make_optional<std::size_t>(length) : std::nullopt, std::chrono::milliseconds(timeout));
        return Status::done;
    }

    Status UartCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        handle = UartHandle{};
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

        return Status::done;
    }

    Status UartCommands::OpenInstance(uint8_t index, const Arguments& arguments)
    {
        UartHandle opened;
        Status status = factory.Open(index, arguments, pins, timeKeeper, opened);
        if (status != Status::done)
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
        return Status::done;
    }

    void UartCommands::SendAsynchronous(infra::ConstByteRange data)
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

    void UartCommands::StartReceive(std::optional<std::size_t> length, infra::Duration timeout)
    {
        if (handle.synchronous != nullptr)
        {
            timeKeeper.Arm(length ? timeout : infra::Duration());
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

    void UartCommands::Received(infra::ConstByteRange data)
    {
        for (auto byte : data)
            if (!received.Full())
                received.AddFromInterrupt(byte);
    }

    void UartCommands::DrainSynchronous(std::size_t wanted)
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

    void UartCommands::CheckReceive()
    {
        if (receiveWanted && received.Size() >= *receiveWanted)
        {
            receiveTimer.Cancel();
            FinishReceive();
        }
    }

    void UartCommands::FinishReceive()
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

    void UartCommands::SendDone(uint32_t generation)
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

    void UartCommands::SendTimeout()
    {
        if (!awaitingSend)
            return;

        awaitingSend = false;
        context.response.Error(Status::timeout);
    }

    void UartCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
