#include "services/hil/commands/HilComparatorCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    namespace
    {
        enum class Edge : uint8_t
        {
            rising,
            falling,
            both,
            off,
        };

        constexpr std::array<HilChoice<Edge>, 4> edges{ {
            { "rising", Edge::rising },
            { "falling", Edge::falling },
            { "both", Edge::both },
            { "off", Edge::off },
        } };

        hal::InterruptTrigger ToTrigger(Edge edge)
        {
            switch (edge)
            {
                case Edge::rising:
                    return hal::InterruptTrigger::risingEdge;
                case Edge::falling:
                    return hal::InterruptTrigger::fallingEdge;
                default:
                    return hal::InterruptTrigger::bothEdges;
            }
        }
    }

    HilComparatorCommands::HilComparatorCommands(HilContext& context, HilComparatorFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, HilOwners::comparator)
        , commands{ {
              HilBind<HilComparatorCommands, &HilComparatorCommands::Open>("comp.open", "<index> [key=value]...", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Read>("comp.read", "<index>", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Interrupt>("comp.irq", "<index> <rising|falling|both|off>", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Count>("comp.count", "<index> [clear=]", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Close>("comp.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilComparatorCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilComparatorCommands::Open(const HilArguments& arguments)
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

    HilStatus HilComparatorCommands::Read(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        const bool output = handle.comparator != nullptr ? handle.comparator->GetOutput() : handle.synchronous->GetOutput();
        context.response.Ok() << " out=" << (output ? 1u : 0u);
        return HilStatus::done;
    }

    HilStatus HilComparatorCommands::Interrupt(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        auto edge = Edge::off;
        HilStatus status = instance.Find(arguments);
        arguments.SelectAt(1, edge, edges, status);
        if (status != HilStatus::done)
            return status;

        if (handle.comparator == nullptr)
            return HilStatus::unsupported;

        handle.comparator->Disable();

        if (edge != Edge::off)
            handle.comparator->Enable([this](bool)
                {
                    count.fetch_add(1);
                },
                ToTrigger(edge));

        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilComparatorCommands::Count(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return HilStatus::usage;

        bool clear = false;
        HilStatus status = instance.Find(arguments);
        arguments.Flag("clear", clear, status);
        if (status != HilStatus::done)
            return status;

        context.response.Ok() << " count=" << (clear ? count.exchange(0) : count.load());
        return HilStatus::done;
    }

    HilStatus HilComparatorCommands::Close(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        HilStatus status = instance.Find(arguments);
        if (status != HilStatus::done)
            return status;

        handle = HilComparatorHandle{};
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return HilStatus::done;
    }

    HilStatus HilComparatorCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilComparatorHandle opened;
        HilStatus status = factory.Open(index, arguments, pins, opened);
        if (status != HilStatus::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened.comparator != nullptr || opened.synchronous != nullptr);

        handle = opened;
        count = 0;
        instance.Open(index);
        context.response.Ok();
        return HilStatus::done;
    }

    void HilComparatorCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
