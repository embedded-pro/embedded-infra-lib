#include "services/hil/commands/UnsupportedCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    UnsupportedCommands::UnsupportedCommands(infra::BoundedVector<Command>& commands, Context& context, infra::MemoryRange<const char* const> names)
        : services::TerminalCommands(context.terminal)
        , commands(commands)
    {
        really_assert(names.size() <= commands.max_size());

        for (auto name : names)
            commands.push_back(Bind<UnsupportedCommands, &UnsupportedCommands::Unsupported>(name, "unsupported", *this, context.response));
    }

    infra::MemoryRange<const services::TerminalCommands::Command> UnsupportedCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status UnsupportedCommands::Unsupported(const Arguments&)
    {
        return Status::unsupported;
    }
}
