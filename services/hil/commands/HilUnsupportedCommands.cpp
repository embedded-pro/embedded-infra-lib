#include "services/hil/commands/HilUnsupportedCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilUnsupportedCommands::HilUnsupportedCommands(infra::BoundedVector<Command>& commands, HilContext& context, infra::MemoryRange<const char* const> names)
        : services::TerminalCommands(context.terminal)
        , commands(commands)
    {
        really_assert(names.size() <= commands.max_size());

        for (auto name : names)
            commands.push_back(HilBind<HilUnsupportedCommands, &HilUnsupportedCommands::Unsupported>(name, "unsupported", *this, context.response));
    }

    infra::MemoryRange<const services::TerminalCommands::Command> HilUnsupportedCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilUnsupportedCommands::Unsupported(const HilArguments&) const
    {
        return HilStatus::unsupported;
    }
}
