#include "services/hil/commands/QeiCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
{
    QeiCommands::QeiCommands(Context& context, QeiFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::qei)
        , commands{ {
              Bind<QeiCommands, &QeiCommands::Open>("qei.open", "<index> [key=value]...", *this, context.response),
              Bind<QeiCommands, &QeiCommands::Read>("qei.read", "<index>", *this, context.response),
              Bind<QeiCommands, &QeiCommands::Close>("qei.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> QeiCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status QeiCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return Status::usage;

        uint8_t index = 0;
        Status status = instance.Parse(arguments, index);
        if (status == Status::done)
            status = factory.Prepare(index, arguments);
        if (status != Status::done)
            return status;

        if (instance.Occupied())
            return Status::busy;

        return OpenInstance(index, arguments);
    }

    Status QeiCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        const auto direction = encoder->Direction() == hal::SynchronousQuadratureEncoder::MotionDirection::forward ? "fwd" : "rev";
        context.response.Ok() << " pos=" << encoder->Position() << " dir=" << direction << " speed=" << encoder->Speed() << " res=" << encoder->Resolution();
        return Status::done;
    }

    Status QeiCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        encoder = nullptr;
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return Status::done;
    }

    Status QeiCommands::OpenInstance(uint8_t index, const Arguments& arguments)
    {
        hal::SynchronousQuadratureEncoder* opened = nullptr;
        Status status = factory.Open(index, arguments, pins, opened);
        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened != nullptr);

        encoder = opened;
        instance.Open(index);
        context.response.Ok();
        return Status::done;
    }

    void QeiCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
