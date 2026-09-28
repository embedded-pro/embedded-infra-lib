#include "services/hil/HilSystemCommands.hpp"

namespace services
{
    namespace
    {
        constexpr infra::Duration resetFlushTime = std::chrono::milliseconds(20);
        constexpr uint32_t maximumDelayMs = 600000;
    }

    HilSystemCommands::HilSystemCommands(HilContext& context, const HilBoardInfo& board, hal::Reset& reset)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , board(board)
        , reset(reset)
        , commands{ {
              HilBind<HilSystemCommands, &HilSystemCommands::Ping>("ping", "", *this, context.response),
              HilBind<HilSystemCommands, &HilSystemCommands::Info>("info", "", *this, context.response),
              HilBind<HilSystemCommands, &HilSystemCommands::Reset>("reset", "", *this, context.response),
              HilBind<HilSystemCommands, &HilSystemCommands::Delay>("delay", "<ms>", *this, context.response),
              HilBind<HilSystemCommands, &HilSystemCommands::BoardPins>("board.pins", "", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilSystemCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    void HilSystemCommands::PrintBoot()
    {
        context.response.Event("boot") << " board=" << board.Name() << " family=" << board.Family() << " sysclk=" << board.SystemClock() << " reset=" << board.ResetCause();
    }

    HilStatus HilSystemCommands::Ping(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilSystemCommands::Info(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        auto uid = board.UniqueId();
        auto line = context.response.Ok();
        line << " board=" << board.Name() << " family=" << board.Family() << " sysclk=" << board.SystemClock() << " reset=" << board.ResetCause() << " uid=";

        if (uid.empty())
            line << "none";
        else
            line.Hex(uid);

        return HilStatus::done;
    }

    HilStatus HilSystemCommands::Reset(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        resetTimer.Start(resetFlushTime, [this]()
            {
                reset.ResetModule("reset");
            });

        return HilStatus::done;
    }

    HilStatus HilSystemCommands::Delay(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        uint32_t milliseconds = 0;
        HilStatus status = HilStatus::done;
        arguments.NumberAt(0, milliseconds, 0, maximumDelayMs, status);
        if (status != HilStatus::done)
            return status;

        if (timer.Armed())
            return HilStatus::busy;

        timer.Start(std::chrono::milliseconds(milliseconds), [this]()
            {
                context.response.Ok();
            });

        return HilStatus::done;
    }

    HilStatus HilSystemCommands::BoardPins(const HilArguments& arguments)
    {
        if (!arguments.Shape(0, 0, {}))
            return HilStatus::usage;

        auto line = context.response.Ok();
        const char* separator = " ";

        for (const auto& alias : context.naming.Aliases())
        {
            line << separator << alias.name << "=";
            line.Pin(alias.pin, context.naming);
            separator = ",";
        }

        return HilStatus::done;
    }
}
