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

    void TerminalWithStorage::PrintDescription(infra::BoundedConstString description, infra::BoundedConstString shortName, infra::BoundedConstString longName, std::size_t descriptionWidth)
    {
        bool firstLine = true;

        while (!description.empty())
        {
            auto chunkSize = std::min(description.size(), descriptionWidth);
            infra::BoundedConstString chunk = description.substr(0, chunkSize);
            description = description.substr(chunkSize);

            infra::BoundedString::WithStorage<40> paddedChunk(chunk);
            paddedChunk.resize(descriptionWidth, ' ');

            if (std::exchange(firstLine, false))
                tracer.Continue() << "  | " << shortName << " | " << longName << " | " << paddedChunk << " |" << infra::endl;
            else
                tracer.Continue() << "  |            |                                  | " << paddedChunk << " |" << infra::endl;
        }
    }

    void TerminalWithStorage::Help()
    {
        constexpr std::size_t descriptionWidth = 40;
        tracer.Continue() << "\r\nAvailable commands:\r\n";
        tracer.Continue() << "  +------------+----------------------------------+------------------------------------------+" << infra::endl;
        tracer.Continue() << "  | Short Name | Long Name                        | Description                              |" << infra::endl;
        tracer.Continue() << "  +------------+----------------------------------+------------------------------------------+" << infra::endl;
        for (const auto& cmd : Commands())
        {
            infra::BoundedString::WithStorage<32> tmpShortName(cmd.info.shortName);
            infra::BoundedString::WithStorage<32> tmpLongName(cmd.info.longName);
            tmpShortName.resize(10, ' ');
            tmpLongName.resize(32, ' ');

            PrintDescription(cmd.info.description, tmpShortName, tmpLongName, descriptionWidth);
        }
        tracer.Continue() << "  +------------+----------------------------------+------------------------------------------+" << infra::endl;
    }
}
