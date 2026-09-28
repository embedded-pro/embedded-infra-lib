#include "services/hil/commands/GpioCommands.hpp"

namespace services::hil
{
    namespace
    {
        enum class Mode : uint8_t
        {
            input,
            output,
            openDrain,
        };

        enum class Edge : uint8_t
        {
            rising,
            falling,
            both,
            off,
        };

        constexpr std::array<Choice<Mode>, 3> modes{ {
            { "in", Mode::input },
            { "out", Mode::output },
            { "od", Mode::openDrain },
        } };

        constexpr std::array<Choice<Pull>, 3> pulls{ {
            { "none", Pull::none },
            { "up", Pull::up },
            { "down", Pull::down },
        } };

        constexpr std::array<Choice<Edge>, 4> edges{ {
            { "rising", Edge::rising },
            { "falling", Edge::falling },
            { "both", Edge::both },
            { "off", Edge::off },
        } };

        constexpr std::array<Choice<hal::InterruptType>, 2> interruptTypes{ {
            { "immediate", hal::InterruptType::immediate },
            { "dispatched", hal::InterruptType::dispatched },
        } };

        constexpr std::array<Choice<bool>, 2> levels{ {
            { "0", false },
            { "1", true },
        } };

        constexpr uint32_t maximumPulses = 1000000;
        constexpr uint32_t maximumPulsePeriodMs = 60000;

        std::optional<hal::InterruptTrigger> ToTrigger(Edge edge)
        {
            switch (edge)
            {
                case Edge::rising:
                    return hal::InterruptTrigger::risingEdge;
                case Edge::falling:
                    return hal::InterruptTrigger::fallingEdge;
                case Edge::both:
                    return hal::InterruptTrigger::bothEdges;
                default:
                    return std::nullopt;
            }
        }
    }

    GpioCommands::GpioCommands(infra::MemoryRange<Entry> entries, Context& context)
        : services::TerminalCommands(context.terminal)
        , entries(entries)
        , context(context)
        , commands{ {
              Bind<GpioCommands, &GpioCommands::Configure>("gpio.cfg", "<pin> <in|out|od> [pull=] [drive=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Set>("gpio.set", "<pin> <0|1>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Get>("gpio.get", "<pin>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Pulse>("gpio.pulse", "<pin> <count> <periodMs>", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Interrupt>("gpio.irq", "<pin> <rising|falling|both|off> [type=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Count>("gpio.count", "<pin> [clear=]", *this, context.response),
              Bind<GpioCommands, &GpioCommands::Release>("gpio.release", "<pin>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> GpioCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    Status GpioCommands::Configure(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "pull", "drive" }))
            return Status::usage;

        PinId id{};
        bool output = false;
        PinOptions options;
        Status status = ParseConfiguration(arguments, id, output, options);
        if (status != Status::done)
            return status;

        Entry* entry = nullptr;
        status = Allocate(id, entry);
        if (status == Status::done)
            status = context.pins.Claim(id, owner::gpio, PinPool::Use::exclusive, entry->pin, options);
        if (status != Status::done)
            return status;

        entry->id = id;
        entry->output = output;
        entry->count = 0;

        if (output)
            entry->pin->Config(hal::PinConfigType::output, options.openDrain);
        else
            entry->pin->Config(hal::PinConfigType::input);

        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Set(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return Status::usage;

        Entry* entry = nullptr;
        bool level = false;
        Status status = Find(arguments, entry);
        arguments.SelectAt(1, level, levels, status);
        if (status != Status::done)
            return status;

        entry->pin->Set(level);
        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Get(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Entry* entry = nullptr;
        Status status = Find(arguments, entry);
        if (status != Status::done)
            return status;

        context.response.Ok() << " value=" << (entry->pin->Get() ? 1u : 0u);
        return Status::done;
    }

    Status GpioCommands::Pulse(const Arguments& arguments)
    {
        if (!arguments.Shape(3, 3, {}))
            return Status::usage;

        Entry* entry = nullptr;
        uint32_t count = 0;
        uint32_t period = 0;
        Status status = Find(arguments, entry);
        arguments.NumberAt(1, count, 1, maximumPulses, status);
        arguments.NumberAt(2, period, 1, maximumPulsePeriodMs, status);
        if (status != Status::done)
            return status;

        if (!entry->output)
            return Status::usage;

        if (pulseEntry != nullptr)
            return Status::busy;

        pulseEntry = entry;
        pulsesRemaining = count;
        pulseTimer.Start(std::chrono::milliseconds(period), [this]()
            {
                Toggle();
            });

        return Status::done;
    }

    Status GpioCommands::Interrupt(const Arguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "type" }))
            return Status::usage;

        Entry* entry = nullptr;
        auto edge = Edge::off;
        auto type = hal::InterruptType::dispatched;
        Status status = Find(arguments, entry);
        arguments.SelectAt(1, edge, edges, status);
        arguments.Select("type", type, interruptTypes, status);
        if (status != Status::done)
            return status;

        if (!context.pins.Factory().SupportsInterrupt(*entry->id))
            return Status::unsupported;

        Rearm(*entry, ToTrigger(edge), type);
        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::Count(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return Status::usage;

        Entry* entry = nullptr;
        bool clear = false;
        Status status = Find(arguments, entry);
        arguments.Flag("clear", clear, status);
        if (status != Status::done)
            return status;

        uint32_t count = clear ? entry->count.exchange(0) : entry->count.load();
        context.response.Ok() << " count=" << count;
        return Status::done;
    }

    Status GpioCommands::Release(const Arguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return Status::usage;

        Entry* entry = nullptr;
        Status status = Find(arguments, entry);
        if (status != Status::done)
            return status;

        if (entry == pulseEntry)
            return Status::busy;

        Free(*entry);
        context.response.Ok();
        return Status::done;
    }

    Status GpioCommands::ParseConfiguration(const Arguments& arguments, PinId& id, bool& output, PinOptions& options) const
    {
        auto mode = Mode::input;
        Status status = Status::done;
        arguments.PinAt(0, context.naming, id, options.pull, status);
        arguments.SelectAt(1, mode, modes, status);
        arguments.Select("pull", options.pull, pulls, status);

        if (auto drive = arguments.Key("drive"); drive && status == Status::done)
        {
            if (auto parsed = context.pins.Factory().ParseDrive(*drive))
                options.drive = *parsed;
            else
                status = Status::usage;
        }

        if (status == Status::done && mode == Mode::openDrain)
        {
            if (arguments.Has("pull") && options.pull != Pull::none)
                return Status::usage;

            options.pull = Pull::none;
            options.openDrain = true;
        }

        output = mode != Mode::input;
        return status;
    }

    Status GpioCommands::Allocate(PinId id, Entry*& entry)
    {
        for (auto& candidate : entries)
            if (candidate.id == id)
                entry = &candidate;

        if (entry != nullptr)
        {
            if (entry == pulseEntry)
                return Status::busy;

            Free(*entry);
            return Status::done;
        }

        for (auto& candidate : entries)
            if (!candidate.id && entry == nullptr)
                entry = &candidate;

        if (entry == nullptr)
            return Status::busy;

        return Status::done;
    }

    Status GpioCommands::Find(const Arguments& arguments, Entry*& entry)
    {
        PinId id{};
        Status status = Status::done;
        arguments.PinAt(0, context.naming, id, status);
        if (status != Status::done)
            return status;

        for (auto& candidate : entries)
            if (candidate.id == id)
            {
                entry = &candidate;
                return Status::done;
            }

        return Status::notOpen;
    }

    void GpioCommands::Rearm(Entry& entry, std::optional<hal::InterruptTrigger> trigger, hal::InterruptType type)
    {
        if (entry.interruptEnabled)
        {
            entry.pin->DisableInterrupt();
            entry.interruptEnabled = false;
        }

        if (trigger)
        {
            auto counter = &entry.count;
            entry.pin->EnableInterrupt([counter]()
                {
                    counter->fetch_add(1, std::memory_order_relaxed);
                },
                *trigger, type);
            entry.interruptEnabled = true;
        }
    }

    void GpioCommands::Free(Entry& entry)
    {
        if (entry.interruptEnabled)
            entry.pin->DisableInterrupt();

        entry.pin->ResetConfig();
        context.pins.Release(*entry.id, owner::gpio);
        entry.id = std::nullopt;
        entry.pin = nullptr;
        entry.interruptEnabled = false;
    }

    void GpioCommands::Toggle()
    {
        pulseEntry->pin->Set(!pulseEntry->pin->GetOutputLatch());

        if (--pulsesRemaining == 0)
        {
            pulseTimer.Cancel();
            pulseEntry = nullptr;
            context.response.Ok();
        }
    }
}
