#include "services/hil/commands/HilComparatorCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    HilComparatorCommands::HilComparatorCommands(HilContext& context, HilComparatorFactory& factory)
        : HilSingleInstanceGroup(context, factory, HilOwners::comparator)
        , factory(factory)
        , commands{ {
              OpenCommand("comp.open", "<index> [key=value]..."),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Read>("comp.read", "<index>", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Interrupt>("comp.irq", "<index> <rising|falling|both|off>", *this, context.response),
              HilBind<HilComparatorCommands, &HilComparatorCommands::Count>("comp.count", "<index> [clear=]", *this, context.response),
              CloseCommand("comp.close", "<index>"),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilComparatorCommands::Commands()
    {
        return infra::MakeRange(commands);
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

        auto edge = HilEdge::off;
        HilStatus status = instance.Find(arguments);
        arguments.SelectAt(1, edge, hilEdges, status);
        if (status != HilStatus::done)
            return status;

        if (handle.comparator == nullptr)
            return HilStatus::unsupported;

        handle.comparator->Disable();

        if (auto trigger = ToTrigger(edge))
            handle.comparator->Enable([this](bool)
                {
                    count.Increment();
                },
                *trigger);

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

        context.response.Ok() << " count=" << count.Read(clear);
        return HilStatus::done;
    }

    HilStatus HilComparatorCommands::OpenInstance(uint8_t index, const HilArguments& arguments)
    {
        HilComparatorHandle opened;
        HilStatus status = factory.Open(index, arguments, pins, opened);
        if (status != HilStatus::done)
            return status;

        really_assert(opened.comparator != nullptr || opened.synchronous != nullptr);

        handle = opened;
        count.Reset();
        return HilStatus::done;
    }

    void HilComparatorCommands::CloseInstance()
    {
        handle = HilComparatorHandle{};
    }
}
