#include "services/hil/commands/EepromCommands.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services::hil
{
    namespace
    {
        constexpr infra::Duration operationTimeout = std::chrono::seconds(5);
    }

    EepromCommands::EepromCommands(infra::ByteRange buffer, Context& context, EepromFactory& factory)
        : services::TerminalCommands(context.terminal)
        , buffer(buffer)
        , context(context)
        , factory(factory)
        , commands{ {
              Bind<EepromCommands, &EepromCommands::Write>("eeprom.write", "<address> <hex>", *this, context.response),
              Bind<EepromCommands, &EepromCommands::Read>("eeprom.read", "<address> <len>", *this, context.response),
              Bind<EepromCommands, &EepromCommands::Erase>("eeprom.erase", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> EepromCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status EepromCommands::Write(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        auto& eeprom = factory.Instance();
        uint32_t address = 0;
        std::size_t size = 0;
        Status status = Status::done;
        arguments.NumberAt(0, address, 0, eeprom.Size(), status);
        if (status == Status::done)
            status = ParseHex(arguments.Positional(1), buffer, size);
        if (status != Status::done)
            return status;

        if (size == 0)
            return Status::usage;

        if (size > eeprom.Size() - address)
            return Status::range;

        if (operating)
            return Status::busy;

        readData = infra::ByteRange();
        eeprom.WriteBuffer(infra::Head(infra::ConstByteRange(buffer), size), address, Start());
        return Status::done;
    }

    Status EepromCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        auto& eeprom = factory.Instance();
        uint32_t address = 0;
        uint32_t length = 0;
        Status status = Status::done;
        arguments.NumberAt(0, address, 0, eeprom.Size(), status);
        arguments.NumberAt(1, length, 1, static_cast<uint32_t>(buffer.size()), status);
        if (status != Status::done)
            return status;

        if (length > eeprom.Size() - address)
            return Status::range;

        if (operating)
            return Status::busy;

        readData = infra::Head(buffer, length);
        eeprom.ReadBuffer(readData, address, Start());
        return Status::done;
    }

    Status EepromCommands::Erase(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        if (operating)
            return Status::busy;

        readData = infra::ByteRange();
        factory.Instance().Erase(Start());
        return Status::done;
    }

    infra::Function<void()> EepromCommands::Start()
    {
        operating = true;
        awaiting = true;
        timer.Start(operationTimeout, [this]()
            {
                Timeout();
            });

        return [this]()
        {
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    Done();
                });
        };
    }

    void EepromCommands::Done()
    {
        operating = false;

        if (!awaiting)
            return;

        awaiting = false;
        timer.Cancel();

        if (readData.empty())
            context.response.Ok();
        else
            (context.response.Ok() << " data=").Hex(readData);
    }

    void EepromCommands::Timeout()
    {
        if (!awaiting)
            return;

        awaiting = false;
        context.response.Error(Status::timeout);
    }
}
