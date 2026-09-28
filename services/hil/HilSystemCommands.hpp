#ifndef SERVICES_HIL_SYSTEM_COMMANDS_HPP
#define SERVICES_HIL_SYSTEM_COMMANDS_HPP

#include "hal/interfaces/Reset.hpp"
#include "infra/timer/Timer.hpp"
#include "services/hil/HilBoardInfo.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services
{
    class HilSystemCommands
        : public services::TerminalCommands
    {
    public:
        HilSystemCommands(HilContext& context, const HilBoardInfo& board, hal::Reset& reset);

        infra::MemoryRange<const Command> Commands() override;

        void PrintBoot();

    private:
        HilStatus Ping(const HilArguments& arguments);
        HilStatus Info(const HilArguments& arguments);
        HilStatus Reset(const HilArguments& arguments);
        HilStatus Delay(const HilArguments& arguments);
        HilStatus BoardPins(const HilArguments& arguments);

    private:
        HilContext& context;
        const HilBoardInfo& board;
        hal::Reset& reset;
        infra::TimerSingleShot timer;
        infra::TimerSingleShot resetTimer;
        std::array<Command, 5> commands;
    };
}

#endif
