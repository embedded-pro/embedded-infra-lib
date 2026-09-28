#include "services/hil/commands/HilQeiCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilQeiCommands::HilQeiCommands(HilContext& context, HilQeiFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::qei)
        , commands{ {
              HilBind<HilQeiCommands, &HilQeiCommands::Open>("qei.open", "<index> [key=value]...", *this, context.response),
              HilBind<HilQeiCommands, &HilQeiCommands::Read>("qei.read", "<index>", *this, context.response),
              HilBind<HilQeiCommands, &HilQeiCommands::Close>("qei.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilQeiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilQeiCommands::Open(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return HilStatus::usage;

        uint8_t index = 0;
        HilStatus status = instance.Parse(arguments, index);
        if (status == HilStatus::done)
            status = factory.Prepare(index, arguments);
        if (status != HilStatus::done)
            return status;

        if (instance.Occupied())
            return HilStatus::busy;

        return OpenInstance(index, arguments);
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

    HilStatus HilQeiCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        encoder = nullptr;
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    HilStatus HilQeiCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        hal::SynchronousQuadratureEncoder* opened = nullptr;
        HilStatus status = factory.Open(index, arguments, pins, opened);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened != nullptr);

        encoder = opened;
        instance.Open(index);
        context.response.Ok();
        return HilStatus::done;
    }

    void HilQeiCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
