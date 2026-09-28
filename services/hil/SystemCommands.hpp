#ifndef SERVICES_HIL_SYSTEM_COMMANDS_HPP
#define SERVICES_HIL_SYSTEM_COMMANDS_HPP

#include "hal/interfaces/Reset.hpp"
#include "infra/timer/Timer.hpp"
#include "services/hil/BoardInfo.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services::hil
{
    class SystemCommands
        : public services::TerminalCommands
    {
    public:
        SystemCommands(Context& context, const BoardInfo& board, hal::Reset& reset);

        infra::MemoryRange<const Command> Commands() override;

        void PrintBoot();

    private:
        Status Ping(const Arguments& arguments);
        Status Info(const Arguments& arguments);
        Status Reset(const Arguments& arguments);
        Status Delay(const Arguments& arguments);
        Status BoardPins(const Arguments& arguments);

    private:
        Context& context;
        const BoardInfo& board;
        hal::Reset& reset;
        infra::TimerSingleShot timer;
        infra::TimerSingleShot resetTimer;
        std::array<Command, 5> commands;
    };
}

#endif
