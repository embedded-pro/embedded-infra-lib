#ifndef SERVICES_HIL_UNSUPPORTED_COMMANDS_HPP
#define SERVICES_HIL_UNSUPPORTED_COMMANDS_HPP

#include "infra/util/BoundedVector.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/util/Terminal.hpp"

namespace services
{
    class HilUnsupportedCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t MaxCommands>
        using WithMaxCommands = infra::WithStorage<HilUnsupportedCommands, infra::BoundedVector<Command>::WithMaxSize<MaxCommands>>;

        HilUnsupportedCommands(infra::BoundedVector<Command>& commands, HilContext& context, infra::MemoryRange<const char* const> names);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Unsupported(const HilArguments& arguments) const;

    private:
        infra::BoundedVector<Command>& commands;
    };
}

#endif
