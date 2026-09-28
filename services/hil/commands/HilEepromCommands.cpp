#include "services/hil/commands/HilEepromCommands.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    namespace
    {
        constexpr infra::Duration operationTimeout = std::chrono::seconds(5);
    }

    HilEepromCommands::HilEepromCommands(infra::ByteRange buffer, HilContext& context, HilEepromFactory& factory)
        : services::TerminalCommands(context.terminal)
        , buffer(buffer)
        , context(context)
        , factory(factory)
        , operation(context.response)
        , commands{ {
              HilBind<HilEepromCommands, &HilEepromCommands::Write>("eeprom.write", "<address> <hex>", *this, context.response),
              HilBind<HilEepromCommands, &HilEepromCommands::Read>("eeprom.read", "<address> <len>", *this, context.response),
              HilBind<HilEepromCommands, &HilEepromCommands::Erase>("eeprom.erase", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilEepromCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilEepromCommands::Write(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        auto& eeprom = factory.Instance();
        uint32_t address = 0;
        std::size_t size = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, address, 0, eeprom.Size(), status);
        if (status == HilStatus::done)
            status = HilArguments::ParseHex(arguments.Positional(1), buffer, size);
        if (status != HilStatus::done)
            return status;

        if (size == 0)
            return HilStatus::usage;

        if (size > eeprom.Size() - address)
            return HilStatus::range;

        if (operation.Busy())
            return HilStatus::busy;

        readData = infra::ByteRange();
        eeprom.WriteBuffer(infra::Head(infra::ConstByteRange(buffer), size), address, Start());
        return HilStatus::done;
    }

    HilStatus HilEepromCommands::Read(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        auto& eeprom = factory.Instance();
        uint32_t address = 0;
        uint32_t length = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, address, 0, eeprom.Size(), status);
        arguments.NumberAt(1, length, 1, static_cast<uint32_t>(buffer.size()), status);
        if (status != HilStatus::done)
            return status;

        if (length > eeprom.Size() - address)
            return HilStatus::range;

        if (operation.Busy())
            return HilStatus::busy;

        readData = infra::Head(buffer, length);
        eeprom.ReadBuffer(readData, address, Start());
        return HilStatus::done;
    }

    HilStatus HilEepromCommands::Erase(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        if (operation.Busy())
            return HilStatus::busy;

        readData = infra::ByteRange();
        factory.Instance().Erase(Start());
        return HilStatus::done;
    }

    infra::Function<void()> HilEepromCommands::Start()
    {
        const auto current = operation.Start(operationTimeout);

        return [this, current]()
        {
            infra::EventDispatcher::Instance().Schedule([this, current]()
                {
                    if (operation.Complete(current))
                        Report();
                });
        };
    }

    void HilEepromCommands::Report() const
    {
        if (readData.empty())
            context.response.Ok();
        else
            (context.response.Ok() << " data=").Hex(readData);
    }
}
