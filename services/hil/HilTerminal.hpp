#ifndef SERVICES_HIL_HIL_TERMINAL_HPP
#define SERVICES_HIL_HIL_TERMINAL_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "infra/util/BoundedDeque.hpp"
#include "infra/util/BoundedString.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Response.hpp"
#include "services/tracer/Tracer.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace services::hil
{
    class HilTerminal
        : public services::TerminalWithCommandsImpl
    {
    public:
        using History = infra::BoundedDeque<infra::BoundedString::WithStorage<services::TerminalWithCommandsImpl::MaxBuffer>>;

        template<std::size_t MaxQueueSize, std::size_t MaxHistory>
        using WithMaxQueueAndMaxHistory = infra::WithStorage<infra::WithStorage<HilTerminal, std::array<uint8_t, MaxQueueSize + 1>>, typename History::template WithMaxSize<MaxHistory>>;

        HilTerminal(infra::MemoryRange<uint8_t> bufferQueue, History& history, hal::SerialCommunication& communication, services::Tracer& tracer, Response& response);

    protected:
        void OnCommandStart() override;
        void OnCommandEnd() override;
        void OnUnrecognizedCommand() override;

    private:
        Response& response;
    };
}

#endif
