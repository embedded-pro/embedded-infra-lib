#include "services/hil/commands/CanCommands.hpp"

namespace services::hil
{
    namespace
    {
        constexpr uint32_t maximumStandardId = 0x7ff;
        constexpr uint32_t maximumExtendedId = 0x1fffffff;
        constexpr std::size_t maximumData = 8;
        constexpr infra::Duration sendTimeout = std::chrono::milliseconds(1000);
        constexpr infra::Duration errorRepeatInterval = std::chrono::milliseconds(100);
    }

    CanCommands::CanCommands(Context& context, CanFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::can)
        , commands{ {
              Bind<CanCommands, &CanCommands::Open>("can.open", "<index> [key=value]...", *this, context.response),
              Bind<CanCommands, &CanCommands::Send>("can.send", "<index> <id> <hex> [ext=]", *this, context.response),
              Bind<CanCommands, &CanCommands::Close>("can.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> CanCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status CanCommands::Open(const Arguments& arguments)
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

    Status CanCommands::Send(const Arguments& arguments)
    {
        if (!arguments.Shape(3, 3, { "ext" }))
            return Status::usage;

        uint32_t id = 0;
        bool extended = false;
        std::array<uint8_t, maximumData> payload{};
        std::size_t size = 0;
        Status status = instance.Find(arguments);
        arguments.Flag("ext", extended, status);
        arguments.NumberAt(1, id, 0, extended ? maximumExtendedId : maximumStandardId, status);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(2), infra::MakeRange(payload), size);
        if (status != Status::done)
            return status;

        if (transmitting)
            return Status::busy;

        Transmit(extended ? hal::Can::Id::Create29BitId(id) : hal::Can::Id::Create11BitId(id), infra::Head(infra::MakeRange(payload), size));
        return Status::done;
    }

    Status CanCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        can = nullptr;
        ++generation;
        transmitting = false;
        awaiting = false;
        timer.Cancel();
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return Status::done;
    }

    Status CanCommands::OpenInstance(uint8_t index, const Arguments& arguments)
    {
        hal::Can* opened = nullptr;
        Status status = factory.Open(index, arguments, pins, [this](const char* error)
            {
                Error(error);
            },
            opened);

        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        can = opened;
        lastError = nullptr;
        instance.Open(index);
        can->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                Received(id, data);
            });

        context.response.Ok();
        return Status::done;
    }

    void CanCommands::Transmit(hal::Can::Id id, infra::ConstByteRange data)
    {
        hal::Can::Message message;
        for (auto byte : data)
            message.push_back(byte);

        transmitting = true;
        awaiting = true;
        const auto current = ++generation;
        timer.Start(sendTimeout, [this]()
            {
                SendTimeout();
            });

        can->SendData(id, message, [this, current](bool success)
            {
                SendDone(current, success);
            });
    }

    void CanCommands::Received(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!instance.Occupied())
            return;

        const bool extended = id.Is29BitId();
        (context.response.Event("can") << " index=" << static_cast<uint32_t>(instance.Index()) << " id=" << (extended ? id.Get29BitId() : id.Get11BitId()) << " ext=" << (extended ? 1u : 0u) << " data=")
            .Hex(infra::ConstByteRange(data.begin(), data.end()));
    }

    void CanCommands::Error(const char* error)
    {
        if (!instance.Occupied())
            return;

        const auto now = infra::Now();
        if (lastError != nullptr && infra::BoundedConstString(lastError) == error && now - lastErrorTime < errorRepeatInterval)
            return;

        lastError = error;
        lastErrorTime = now;
        context.response.Event("can") << " index=" << static_cast<uint32_t>(instance.Index()) << " error=" << error;
    }

    void CanCommands::SendDone(uint32_t current, bool success)
    {
        if (current != generation)
            return;

        transmitting = false;

        if (awaiting)
        {
            awaiting = false;
            timer.Cancel();

            if (success)
                context.response.Ok();
            else
                context.response.Error(Status::failed);
        }
    }

    void CanCommands::SendTimeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }

    void CanCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
