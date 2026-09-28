#include "services/hil/commands/HilPwmCommands.hpp"
#include <limits>

namespace services
{
    HilPwmCommands::HilPwmCommands(HilContext& context, HilPwmFactory& factory)
        : HilSingleInstanceGroup(context, factory, HilOwners::pwm)
        , factory(factory)
        , commands{ {
              OpenCommand("pwm.open", "<module> [key=value]..."),
              HilBind<HilPwmCommands, &HilPwmCommands::Duty>("pwm.duty", "<module> <duty%>...", *this, Context().response),
              HilBind<HilPwmCommands, &HilPwmCommands::Frequency>("pwm.freq", "<module> <hz>", *this, Context().response),
              HilBind<HilPwmCommands, &HilPwmCommands::Stop>("pwm.stop", "<module>", *this, Context().response),
              CloseCommand("pwm.close", "<module>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilPwmCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilPwmCommands::Duty(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 1 + maximumChannels, {}))
            return HilStatus::usage;

        HilStatus status = Instance().Find(arguments);
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
        Context().response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::Frequency(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        uint32_t frequency = 0;
        HilStatus status = Instance().Find(arguments);
        arguments.NumberAt(1, frequency, 1, std::numeric_limits<uint32_t>::max(), status);
        if (status == HilStatus::done)
            status = factory.ChangeFrequency(Instance().Index(), frequency);
        if (status != HilStatus::done)
            return status;

        handle->SetBaseFrequency(hal::Hertz(frequency));
        Context().response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::Stop(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = Instance().Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle->Stop();
        Context().response.Ok();
        return HilStatus::done;
    }

    HilStatus HilPwmCommands::OpenInstance(uint8_t moduleIndex, const HilArguments& arguments)
    {
        HilPwmHandle* opened = nullptr;
        HilStatus status = factory.Open(moduleIndex, arguments, Pins(), opened);
        if (status != HilStatus::done)
            return status;

        really_assert(opened != nullptr);

        handle = opened;
        return HilStatus::done;
    }

    void HilPwmCommands::Opened(HilResponse::Line& line) const
    {
        factory.ReportOpened(Instance().Index(), line);
    }

    void HilPwmCommands::CloseInstance()
    {
        handle = nullptr;
    }
}
