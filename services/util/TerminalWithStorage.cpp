#include "services/util/TerminalWithStorage.hpp"

namespace services
{
    TerminalWithStorage::TerminalWithStorage(infra::BoundedVector<services::TerminalWithStorage::Command>& storage, services::TerminalWithCommands& terminal, services::Tracer& tracer)
        : services::TerminalCommands(terminal)
        , commands(storage)
        , tracer(tracer)
    {
        AddCommand({ { "help", "h", "Print help menu" }, [this](const infra::BoundedConstString&)
            {
                Help();
            } });
    }

    infra::MemoryRange<const services::TerminalWithStorage::Command> TerminalWithStorage::Commands()
    {
        return infra::MakeRange(commands);
    }

    void TerminalWithStorage::AddCommand(const Command& command)
    {
        really_assert(std::find_if(commands.begin(), commands.end(), [&command](auto& it)
                          {
                              return command.info.longName == it.info.longName || command.info.shortName == it.info.shortName;
                          }) == commands.end());

        commands.push_back(command);
    }

    void TerminalWithStorage::ProcessResult(const StatusWithMessage& result)
    {
        if (result.result != Status::success)
            tracer.Trace() << "ERROR: " << result.message;
    }

    void TerminalWithStorage::Help()
    {
        for (const auto& cmd : Commands())
        {
            infra::BoundedString::WithStorage<128> tmpLongName(cmd.info.longName);
            tmpLongName.resize(32, ' ');
            tracer.Continue() << cmd.info.shortName << "\t" << tmpLongName << "\t" << cmd.info.description << infra::endl;
        }
    }
}
