#ifndef SERVICES_HIL_UNSUPPORTED_COMMANDS_HPP
#define SERVICES_HIL_UNSUPPORTED_COMMANDS_HPP

#include "infra/util/BoundedVector.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"

namespace services::hil
{
    class UnsupportedCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t MaxCommands>
        using WithMaxCommands = infra::WithStorage<UnsupportedCommands, infra::BoundedVector<Command>::WithMaxSize<MaxCommands>>;

        UnsupportedCommands(infra::BoundedVector<Command>& commands, Context& context, infra::MemoryRange<const char* const> names);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Unsupported(const Arguments& arguments);

    private:
        infra::BoundedVector<Command>& commands;
    };
}

#endif
