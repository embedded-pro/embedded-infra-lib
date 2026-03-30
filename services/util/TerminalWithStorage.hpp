#ifndef SERVICES_TERMINAL_WITH_STORAGE_HPP
#define SERVICES_TERMINAL_WITH_STORAGE_HPP

#include "infra/util/BoundedVector.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"

namespace services
{
    class TerminalWithStorage
        : public services::TerminalCommands
    {
    public:
        using services::TerminalCommands::Command;

        template<std::size_t Max>
        using WithMaxSize = infra::WithStorage<TerminalWithStorage, infra::BoundedVector<Command>::WithMaxSize<Max>>;

        enum class Status : uint8_t
        {
            success,
            error
        };

        struct StatusWithMessage
        {
            Status result = Status::success;
            infra::BoundedConstString message = "";
        };

        TerminalWithStorage(infra::BoundedVector<Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer);

        // implementation of services::TerminalCommands
        infra::MemoryRange<const Command> Commands() override;

        void AddCommand(const Command& command);
        void ProcessResult(const StatusWithMessage& result);

    private:
        void Help();
        void PrintDescription(infra::BoundedConstString description, infra::BoundedConstString shortName, infra::BoundedConstString longName, std::size_t descriptionWidth);

    private:
        infra::BoundedVector<Command>& commands;
        services::Tracer& tracer;
    };
}

#endif
