#include "drivers/display/ili9341/Ili9341.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <chrono>

namespace drivers
{
    Ili9341::Ili9341(services::RegisterBusAccess& bus, const Panel& panel, const infra::Function<void()>& onInitialized)
        : bus(bus)
        , panel(panel)
        , initialized(onInitialized)
    {
        really_assert(!panel.commands.empty());

        WriteNextCommand();
    }

    void Ili9341::WriteNextCommand()
    {
        if (commandIndex == panel.commands.size())
        {
            initialized();
            return;
        }

        const Command& command = panel.commands[commandIndex];
        bus.WriteRegister(command.command, command.parameters, [this]()
            {
                CommandWritten();
            });
    }

    void Ili9341::CommandWritten()
    {
        uint16_t delayInMilliseconds = panel.commands[commandIndex++].delayAfterInMilliseconds;

        if (delayInMilliseconds == 0)
            WriteNextCommand();
        else
            timer.Start(std::chrono::milliseconds(delayInMilliseconds), [this]()
                {
                    WriteNextCommand();
                });
    }
}
