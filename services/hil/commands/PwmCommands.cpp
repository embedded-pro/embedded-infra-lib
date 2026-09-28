#include "services/hil/commands/PwmCommands.hpp"
#include <limits>

namespace services::hil
{
    PwmCommands::PwmCommands(Context& context, PwmFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::pwm)
        , commands{ {
              Bind<PwmCommands, &PwmCommands::Open>("pwm.open", "<module> [key=value]...", *this, context.response),
              Bind<PwmCommands, &PwmCommands::Duty>("pwm.duty", "<module> <duty%>...", *this, context.response),
              Bind<PwmCommands, &PwmCommands::Frequency>("pwm.freq", "<module> <hz>", *this, context.response),
              Bind<PwmCommands, &PwmCommands::Stop>("pwm.stop", "<module>", *this, context.response),
              Bind<PwmCommands, &PwmCommands::Close>("pwm.close", "<module>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> PwmCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status PwmCommands::Open(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return Status::usage;

        uint8_t module = 0;
        Status status = instance.Parse(arguments, module);
        if (status == Status::done)
            status = factory.Prepare(module, arguments);
        if (status != Status::done)
            return status;

        if (instance.Occupied())
            return Status::busy;

        return OpenInstance(module, arguments);
    }

    Status PwmCommands::Duty(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 1 + maximumChannels, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        std::array<hal::DutyCycle, maximumChannels> dutyCycles;
        const auto count = arguments.PositionalCount() - 1;
        if (count != 1 && count != handle->Channels())
            return Status::usage;

        for (std::size_t i = 0; i != count; ++i)
        {
            auto dutyCycle = ParseDutyCycle(arguments.Positional(i + 1));
            if (!dutyCycle)
                return Status::usage;

            dutyCycles[i] = *dutyCycle;
        }

        handle->Start(infra::Head(infra::MakeRange(std::as_const(dutyCycles)), count));
        context.response.Ok();
        return Status::done;
    }

    Status PwmCommands::Frequency(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        uint32_t frequency = 0;
        Status status = instance.Find(arguments);
        arguments.NumberAt(1, frequency, 1, std::numeric_limits<uint32_t>::max(), status);
        if (status == Status::done)
            status = factory.ChangeFrequency(instance.Index(), frequency);
        if (status != Status::done)
            return status;

        handle->SetBaseFrequency(hal::Hertz(frequency));
        context.response.Ok();
        return Status::done;
    }

    Status PwmCommands::Stop(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        handle->Stop();
        context.response.Ok();
        return Status::done;
    }

    Status PwmCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        handle = nullptr;
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return Status::done;
    }

    Status PwmCommands::OpenInstance(uint8_t module, const Arguments& arguments)
    {
        PwmHandle* opened = nullptr;
        Status status = factory.Open(module, arguments, pins, opened);
        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened != nullptr);

        handle = opened;
        instance.Open(module);
        auto line = context.response.Ok();
        factory.ReportOpened(module, line);
        return Status::done;
    }

    void PwmCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
