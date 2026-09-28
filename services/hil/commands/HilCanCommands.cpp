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
        : HilSingleInstanceGroup(context, factory, HilOwners::can)
        , factory(factory)
        , sending(context.response)
        , commands{ {
              OpenCommand("can.open", "<index> [key=value]..."),
              HilBind<HilCanCommands, &HilCanCommands::Send>("can.send", "<index> <id> <hex> [ext=]", *this, context.response),
              CloseCommand("can.close", "<index>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilCanCommands::Commands()
    {
        return infra::MakeRange(commands);
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

        if (sending.Busy())
            return HilStatus::busy;

        Transmit(extended ? hal::Can::Id::Create29BitId(id) : hal::Can::Id::Create11BitId(id), infra::Head(infra::MakeRange(payload), size));
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
            return status;

        can = opened;
        lastError = nullptr;
        can->ReceiveData([this](hal::Can::Id id, const hal::Can::Message& data)
            {
                Received(id, data);
            });

        return HilStatus::done;
    }

    void HilCanCommands::CloseInstance()
    {
        can = nullptr;
        sending.Cancel();
    }

    void HilCanCommands::Transmit(hal::Can::Id id, infra::ConstByteRange data)
    {
        const auto operation = sending.Start(sendTimeout);

        can->SendData(id, hal::Can::Message(data), [this, operation](bool success)
            {
                if (!sending.Complete(operation))
                    return;

                if (success)
                    context.response.Ok();
                else
                    context.response.Error(HilStatus::failed);
            });
    }

    void HilCanCommands::Received(hal::Can::Id id, const hal::Can::Message& data) const
    {
        if (!instance.Occupied())
            return;

        const bool extended = id.Is29BitId();
        (context.response.Event("can") << " index=" << static_cast<uint32_t>(instance.Index()) << " id=" << (extended ? id.Get29BitId() : id.Get11BitId()) << " ext=" << (extended ? 1u : 0u) << " data=")
            .Hex(infra::MakeRange(data));
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
}
