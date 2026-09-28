#include "services/hil/commands/HilQeiCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilQeiCommands::HilQeiCommands(HilContext& context, HilQeiFactory& factory)
        : HilSingleInstanceGroup(context, factory, HilOwners::qei)
        , factory(factory)
        , commands{ {
              OpenCommand("qei.open", "<index> [key=value]..."),
              HilBind<HilQeiCommands, &HilQeiCommands::Read>("qei.read", "<index>", *this, context.response),
              CloseCommand("qei.close", "<index>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilQeiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilQeiCommands::Read(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        const auto direction = encoder->Direction() == hal::SynchronousQuadratureEncoder::MotionDirection::forward ? "fwd" : "rev";
        context.response.Ok() << " pos=" << encoder->Position() << " dir=" << direction << " speed=" << encoder->Speed() << " res=" << encoder->Resolution();
        return HilStatus::done;
    }

    HilStatus HilQeiCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        hal::SynchronousQuadratureEncoder* opened = nullptr;
        HilStatus status = factory.Open(index, arguments, pins, opened);
        if (status != HilStatus::done)
            return status;

        really_assert(opened != nullptr);

        encoder = opened;
        return HilStatus::done;
    }

    void HilQeiCommands::CloseInstance()
    {
        encoder = nullptr;
    }
}
