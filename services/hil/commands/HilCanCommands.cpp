#include "services/hil/commands/HilCanCommands.hpp"

namespace services
{
    namespace
    {
        constexpr uint32_t maximumStandardId = 0x7ff;
        constexpr uint32_t maximumExtendedId = 0x1fffffff;
        constexpr std::size_t maximumData = 8;
        constexpr infra::Duration sendTimeout = std::chrono::milliseconds(1000);
        constexpr infra::Duration errorRepeatInterval = std::chrono::milliseconds(100);
    }

    HilCanCommands::HilCanCommands(HilContext& context, HilCanFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::can)
        , commands{ {
              HilBind<HilCanCommands, &HilCanCommands::Open>("can.open", "<index> [key=value]...", *this, context.response),
              HilBind<HilCanCommands, &HilCanCommands::Send>("can.send", "<index> <id> <hex> [ext=]", *this, context.response),
              HilBind<HilCanCommands, &HilCanCommands::Close>("can.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilCanCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilCanCommands::Open(const HilArguments& arguments)
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

    HilStatus HilCanCommands::Send(const HilArguments& arguments)
    {
        if (!arguments.Shape(3, 3, { "ext" }))
            return HilStatus::usage;

        uint32_t id = 0;
        bool extended = false;
        std::array<uint8_t, maximumData> payload{};
        std::size_t size = 0;
        HilStatus status = instance.Find(arguments);
        arguments.Flag("ext", extended, status);
        arguments.NumberAt(1, id, 0, extended ? maximumExtendedId : maximumStandardId, status);
        if (status == HilStatus::done)
            status = HilArguments::ParseHex(arguments.Positional(2), infra::MakeRange(payload), size);
        if (status != HilStatus::done)
            return status;

        if (transmitting)
            return HilStatus::busy;

        Transmit(extended ? hal::Can::Id::Create29BitId(id) : hal::Can::Id::Create11BitId(id), infra::Head(infra::MakeRange(payload), size));
        return HilStatus::done;
    }

    HilStatus HilCanCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
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

        return HilStatus::done;
    }

    HilStatus HilCanCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        hal::Can* opened = nullptr;
        HilStatus status = factory.Open(index, arguments, pins, [this](const char* error)
            {
                Error(error);
            },
            opened);

        if (status != HilStatus::done)
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
        return HilStatus::done;
    }

    void HilCanCommands::Transmit(hal::Can::Id id, infra::ConstByteRange data)
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

    void HilCanCommands::Received(hal::Can::Id id, const hal::Can::Message& data)
    {
        if (!instance.Occupied())
            return;

        const bool extended = id.Is29BitId();
        (context.response.Event("can") << " index=" << static_cast<uint32_t>(instance.Index()) << " id=" << (extended ? id.Get29BitId() : id.Get11BitId()) << " ext=" << (extended ? 1u : 0u) << " data=")
            .Hex(infra::ConstByteRange(data.begin(), data.end()));
    }

    void HilCanCommands::Error(const char* error)
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

    void HilCanCommands::SendDone(uint32_t current, bool success)
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
                context.response.Error(HilStatus::failed);
        }
    }

    void HilCanCommands::SendTimeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(HilStatus::timeout);
    }

    void HilCanCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
