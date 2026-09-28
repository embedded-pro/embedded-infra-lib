#include "services/hil/HilTerminal.hpp"

namespace services::hil
{
    HilTerminal::HilTerminal(infra::MemoryRange<uint8_t> bufferQueue, History& history, hal::SerialCommunication& communication, services::Tracer& tracer, Response& response)
        : services::TerminalWithCommandsImpl(bufferQueue, history, communication, tracer)
        , response(response)
    {}

    void HilTerminal::OnCommandStart()
    {
        response.BeginCommand();
    }

    void HilTerminal::OnCommandEnd()
    {
        response.EndCommand();
    }

    void HilTerminal::OnUnrecognizedCommand()
    {
        response.Error(Status::usage);
    }
}
