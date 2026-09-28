#include "services/hil/SystemCommands.hpp"

namespace services::hil
{
    namespace
    {
        constexpr infra::Duration resetFlushTime = std::chrono::milliseconds(20);
        constexpr uint32_t maximumDelayMs = 600000;
    }

    SystemCommands::SystemCommands(Context& context, const BoardInfo& board, hal::Reset& reset)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , board(board)
        , reset(reset)
        , commands{ {
              Bind<SystemCommands, &SystemCommands::Ping>("ping", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Info>("info", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Reset>("reset", "", *this, context.response),
              Bind<SystemCommands, &SystemCommands::Delay>("delay", "<ms>", *this, context.response),
              Bind<SystemCommands, &SystemCommands::BoardPins>("board.pins", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> SystemCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    void SystemCommands::PrintBoot()
    {
        context.response.Event("boot") << " board=" << board.Name() << " family=" << board.Family() << " sysclk=" << board.SystemClock() << " reset=" << board.ResetCause();
    }

    Status SystemCommands::Ping(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        context.response.Ok();
        return Status::done;
    }

    Status SystemCommands::Info(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        auto uid = board.UniqueId();
        auto line = context.response.Ok();
        line << " board=" << board.Name() << " family=" << board.Family() << " sysclk=" << board.SystemClock() << " reset=" << board.ResetCause() << " uid=";

        if (uid.empty())
            line << "none";
        else
            line.Hex(uid);

        return Status::done;
    }

    Status SystemCommands::Reset(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        resetTimer.Start(resetFlushTime, [this]()
            {
                reset.ResetModule("reset");
            });

        return Status::done;
    }

    Status SystemCommands::Delay(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        uint32_t milliseconds = 0;
        Status status = Status::done;
        arguments.NumberAt(0, milliseconds, 0, maximumDelayMs, status);
        if (status != Status::done)
            return status;

        if (timer.Armed())
            return Status::busy;

        timer.Start(std::chrono::milliseconds(milliseconds), [this]()
            {
                context.response.Ok();
            });

        return Status::done;
    }

    Status SystemCommands::BoardPins(const Arguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return Status::usage;

        auto line = context.response.Ok();
        const char* separator = " ";

        for (const auto& alias : context.naming.Aliases())
        {
            line << separator << alias.name << "=";
            line.Pin(alias.pin, context.naming);
            separator = ",";
        }

        return Status::done;
    }
}
