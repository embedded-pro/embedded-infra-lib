#include "services/hil/commands/ComparatorCommands.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services::hil
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

        constexpr std::array<Choice<Edge>, 4> edges{ {
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

    ComparatorCommands::ComparatorCommands(Context& context, ComparatorFactory& factory)
        : services::TerminalCommands(context.terminal)
        , context(context)
        , factory(factory)
        , instance(factory.Instances())
        , pins(context.pins, owner::comparator)
        , commands{ {
              Bind<ComparatorCommands, &ComparatorCommands::Open>("comp.open", "<index> [key=value]...", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Read>("comp.read", "<index>", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Interrupt>("comp.irq", "<index> <rising|falling|both|off>", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Count>("comp.count", "<index> [clear=]", *this, context.response),
              Bind<ComparatorCommands, &ComparatorCommands::Close>("comp.close", "<index>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> ComparatorCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status ComparatorCommands::Open(const Arguments& arguments)
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

    Status ComparatorCommands::Read(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        const bool output = handle.comparator != nullptr ? handle.comparator->GetOutput() : handle.synchronous->GetOutput();
        context.response.Ok() << " out=" << (output ? 1u : 0u);
        return Status::done;
    }

    Status ComparatorCommands::Interrupt(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        auto edge = Edge::off;
        Status status = instance.Find(arguments);
        arguments.SelectAt(1, edge, edges, status);
        if (status != Status::done)
            return status;

        if (handle.comparator == nullptr)
            return Status::unsupported;

        handle.comparator->Disable();

        if (edge != Edge::off)
            handle.comparator->Enable([this](bool)
                {
                    count.fetch_add(1, std::memory_order_relaxed);
                },
                ToTrigger(edge));

        context.response.Ok();
        return Status::done;
    }

    Status ComparatorCommands::Count(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return Status::usage;

        bool clear = false;
        Status status = instance.Find(arguments);
        arguments.Flag("clear", clear, status);
        if (status != Status::done)
            return status;

        context.response.Ok() << " count=" << (clear ? count.exchange(0) : count.load());
        return Status::done;
    }

    Status ComparatorCommands::Close(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Status status = instance.Find(arguments);
        if (status != Status::done)
            return status;

        handle = ComparatorHandle{};
        instance.StartClosing();
        factory.Close(instance.Index(), [this]()
            {
                Closed();
            });

        return Status::done;
    }

    Status ComparatorCommands::OpenInstance(uint8_t index, const Arguments& arguments)
    {
        ComparatorHandle opened;
        Status status = factory.Open(index, arguments, pins, opened);
        if (status != Status::done)
        {
            pins.Release();
            return status;
        }

        really_assert(opened.comparator != nullptr || opened.synchronous != nullptr);

        handle = opened;
        count = 0;
        instance.Open(index);
        context.response.Ok();
        return Status::done;
    }

    void ComparatorCommands::Closed()
    {
        pins.Release();
        instance.Closed();
        context.response.Ok();
    }
}
