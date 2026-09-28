#include "services/hil/commands/HilPwmCommands.hpp"
#include <limits>

namespace services
{
    HilPwmCommands::HilPwmCommands(HilContext& context, HilPwmFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::pwm)
        , commands{ {
              HilBind<HilPwmCommands, &HilPwmCommands::Open>("pwm.open", "<module> [key=value]...", *this, context.response),
              HilBind<HilPwmCommands, &HilPwmCommands::Duty>("pwm.duty", "<module> <duty%>...", *this, context.response),
              HilBind<HilPwmCommands, &HilPwmCommands::Frequency>("pwm.freq", "<module> <hz>", *this, context.response),
              HilBind<HilPwmCommands, &HilPwmCommands::Stop>("pwm.stop", "<module>", *this, context.response),
              HilBind<HilPwmCommands, &HilPwmCommands::Close>("pwm.close", "<module>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilPwmCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilPwmCommands::Open(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, factory.OpenKeys()))
            return HilStatus::usage;

        uint8_t moduleIndex = 0;
        HilStatus status = instance.Parse(arguments, moduleIndex);
        if (status == HilStatus::done)
            status = factory.Prepare(moduleIndex, arguments);
        if (status != HilStatus::done)
            return status;

        if (instance.Occupied())
            return HilStatus::busy;

        return OpenInstance(moduleIndex, arguments);
    }

    HilStatus HilPwmCommands::Duty(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 1 + maximumChannels, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        std::array<hal::DutyCycle, maximumChannels> dutyCycles;
        const auto count = arguments.PositionalCount() - 1;
        if (count != 1 && count != handle->Channels())
            return HilStatus::usage;

        for (std::size_t i = 0; i != count; ++i)
        {
            auto dutyCycle = HilArguments::ParseDutyCycle(arguments.Positional(i + 1));
            if (!dutyCycle)
                return HilStatus::usage;

            dutyCycles[i] = *dutyCycle;
        }

        handle->Start(infra::Head(infra::MakeRange(std::as_const(dutyCycles)), count));
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::Frequency(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        uint32_t frequency = 0;
        HilStatus status = instance.Find(arguments);
        arguments.NumberAt(1, frequency, 1, std::numeric_limits<uint32_t>::max(), status);
        if (status == HilStatus::done)
            status = factory.ChangeFrequency(instance.Index(), frequency);
        if (status != HilStatus::done)
            return status;

        handle->SetBaseFrequency(hal::Hertz(frequency));
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::Stop(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle->Stop();
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle = nullptr;
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    HilStatus HilPwmCommands::OpenInstance(uint8_t moduleIndex, const HilArguments& arguments)
    {
        HilPwmHandle* opened = nullptr;
        HilStatus status = factory.Open(moduleIndex, arguments, pins, opened);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened != nullptr);

        handle = opened;
        instance.Open(moduleIndex);
        auto line = context.response.Ok();
        factory.ReportOpened(moduleIndex, line);
        return HilStatus::done;
    }

    void HilPwmCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
