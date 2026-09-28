#include "services/hil/commands/HilSingleInstanceGroup.hpp"

namespace services
{
    HilSingleInstanceGroup::HilSingleInstanceGroup(HilContext& context, HilInstanceFactory& factory, HilOwner owner)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , instance(factory.Instances())
        , pins(context.pins, owner)
        , factory(factory)
    {}

    services::TerminalCommands::Command HilSingleInstanceGroup::OpenCommand(const char* name, const char* usage)
    {
        return HilBind<HilSingleInstanceGroup, &HilSingleInstanceGroup::Open>(name, usage, *this, context.response);
    }

    services::TerminalCommands::Command HilSingleInstanceGroup::CloseCommand(const char* name, const char* usage)
    {
        return HilBind<HilSingleInstanceGroup, &HilSingleInstanceGroup::Close>(name, usage, *this, context.response);
    }

    void HilSingleInstanceGroup::Opened(HilResponse::Line&) const
    {}

    HilStatus HilSingleInstanceGroup::Open(const HilArguments& arguments)
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

        status = OpenInstance(index, arguments);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        instance.Open(index);
        auto line = context.response.Ok();
        Opened(line);
        return HilStatus::done;
    }

    HilStatus HilSingleInstanceGroup::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        CloseInstance();
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    void HilSingleInstanceGroup::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }

    HilContext& HilSingleInstanceGroup::Context() const
    {
        return context;
    }

    HilSingleInstance& HilSingleInstanceGroup::Instance()
    {
        return instance;
    }

    const HilSingleInstance& HilSingleInstanceGroup::Instance() const
    {
        return instance;
    }

    HilPinOwner& HilSingleInstanceGroup::Pins()
    {
        return pins;
    }
}
