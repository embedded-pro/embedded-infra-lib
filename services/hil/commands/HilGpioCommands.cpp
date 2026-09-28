#include "services/hil/commands/HilGpioCommands.hpp"

namespace services
{
    namespace
    {
        enum class Mode : uint8_t
        {
            input,
            output,
            openDrain,
        };

        constexpr std::array<HilChoice<Mode>, 3> modes{ {
            { "in", Mode::input },
            { "out", Mode::output },
            { "od", Mode::openDrain },
        } };

        constexpr std::array<HilChoice<HilPull>, 3> pulls{ {
            { "none", HilPull::none },
            { "up", HilPull::up },
            { "down", HilPull::down },
        } };

        constexpr std::array<HilChoice<hal::InterruptType>, 2> interruptTypes{ {
            { "immediate", hal::InterruptType::immediate },
            { "dispatched", hal::InterruptType::dispatched },
        } };

        constexpr std::array<HilChoice<bool>, 2> levels{ {
            { "0", false },
            { "1", true },
        } };

        constexpr uint32_t maximumPulses = 1000000;
        constexpr uint32_t maximumPulsePeriodMs = 60000;
    }

    HilGpioCommands::HilGpioCommands(infra::MemoryRange<Entry> entries, HilContext& context)
        : services::TerminalCommands(context.terminal)
        , entries(entries)
        , context(context)
        , commands{ {
              HilBind<HilGpioCommands, &HilGpioCommands::Configure>("gpio.cfg", "<pin> <in|out|od> [pull=] [drive=]", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Set>("gpio.set", "<pin> <0|1>", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Get>("gpio.get", "<pin>", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Pulse>("gpio.pulse", "<pin> <count> <periodMs>", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Interrupt>("gpio.irq", "<pin> <rising|falling|both|off> [type=]", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Count>("gpio.count", "<pin> [clear=]", *this, context.response),
              HilBind<HilGpioCommands, &HilGpioCommands::Release>("gpio.release", "<pin>", *this, context.response),
          } }
    {}

    infra::MemoryRange<const services::TerminalCommands::Command> HilGpioCommands::Commands()
    {
        return infra::MakeRange(commands);
    }

    HilStatus HilGpioCommands::Configure(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "pull", "drive" }))
            return HilStatus::usage;

        HilPinId id{};
        bool output = false;
        HilPinOptions options;
        HilStatus status = ParseConfiguration(arguments, id, output, options);
        if (status != HilStatus::done)
            return status;

        Entry* entry = nullptr;
        status = Allocate(id, entry);
        if (status == HilStatus::done)
            status = context.pins.Claim(id, HilOwners::gpio, HilPinPool::Use::exclusive, entry->pin, options);
        if (status != HilStatus::done)
            return status;

        entry->id = id;
        entry->output = output;
        entry->count.Reset();

        if (output)
            entry->pin->Config(hal::PinConfigType::output, options.openDrain);
        else
            entry->pin->Config(hal::PinConfigType::input);

        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Set(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, {}))
            return HilStatus::usage;

        Entry* entry = nullptr;
        bool level = false;
        HilStatus status = Find(arguments, entry);
        arguments.SelectAt(1, level, levels, status);
        if (status != HilStatus::done)
            return status;

        entry->pin->Set(level);
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Get(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        Entry* entry = nullptr;
        HilStatus status = Find(arguments, entry);
        if (status != HilStatus::done)
            return status;

        context.response.Ok() << " value=" << (entry->pin->Get() ? 1u : 0u);
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Pulse(const HilArguments& arguments)
    {
        if (!arguments.Shape(3, 3, {}))
            return HilStatus::usage;

        Entry* entry = nullptr;
        uint32_t count = 0;
        uint32_t period = 0;
        HilStatus status = Find(arguments, entry);
        arguments.NumberAt(1, count, 1, maximumPulses, status);
        arguments.NumberAt(2, period, 1, maximumPulsePeriodMs, status);
        if (status != HilStatus::done)
            return status;

        if (!entry->output)
            return HilStatus::usage;

        if (pulseEntry != nullptr)
            return HilStatus::busy;

        pulseEntry = entry;
        pulseTimer.Start(count, std::chrono::milliseconds(period), [this]()
            {
                Toggle();
            });

        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Interrupt(const HilArguments& arguments)
    {
        if (!arguments.Shape(2, 2, { "type" }))
            return HilStatus::usage;

        Entry* entry = nullptr;
        auto edge = HilEdge::off;
        auto type = hal::InterruptType::dispatched;
        HilStatus status = Find(arguments, entry);
        arguments.SelectAt(1, edge, hilEdges, status);
        arguments.Select("type", type, interruptTypes, status);
        if (status != HilStatus::done)
            return status;

        if (!context.pins.Factory().SupportsInterrupt(*entry->id))
            return HilStatus::unsupported;

        Rearm(*entry, ToTrigger(edge), type);
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Count(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, { "clear" }))
            return HilStatus::usage;

        Entry* entry = nullptr;
        bool clear = false;
        HilStatus status = Find(arguments, entry);
        arguments.Flag("clear", clear, status);
        if (status != HilStatus::done)
            return status;

        context.response.Ok() << " count=" << entry->count.Read(clear);
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Release(const HilArguments& arguments)
    {
        if (!arguments.Shape(1, 1, {}))
            return HilStatus::usage;

        Entry* entry = nullptr;
        HilStatus status = Find(arguments, entry);
        if (status != HilStatus::done)
            return status;

        if (entry == pulseEntry)
            return HilStatus::busy;

        Free(*entry);
        context.response.Ok();
        return HilStatus::done;
    }

    HilStatus HilGpioCommands::ParseConfiguration(const HilArguments& arguments, HilPinId& id, bool& output, HilPinOptions& options) const
    {
        auto mode = Mode::input;
        HilStatus status = HilStatus::done;
        arguments.PinAt(0, context.naming, id, options.pull, status);
        arguments.SelectAt(1, mode, modes, status);
        arguments.Select("pull", options.pull, pulls, status);

        if (auto drive = arguments.Key("drive"); drive && status == HilStatus::done)
        {
            if (auto parsed = context.pins.Factory().ParseDrive(*drive); parsed.has_value())
                options.drive = *parsed;
            else
                status = HilStatus::usage;
        }

        if (status == HilStatus::done && mode == Mode::openDrain)
        {
            if (arguments.Has("pull") && options.pull != HilPull::none)
                return HilStatus::usage;

            options.pull = HilPull::none;
            options.openDrain = true;
        }

        output = mode != Mode::input;
        return status;
    }

    HilStatus HilGpioCommands::Allocate(HilPinId id, Entry*& entry)
    {
        for (auto& candidate : entries)
            if (candidate.id == id)
                entry = &candidate;

        if (entry != nullptr)
        {
            if (entry == pulseEntry)
                return HilStatus::busy;

            Free(*entry);
            return HilStatus::done;
        }

        for (auto& candidate : entries)
            if (!candidate.id && entry == nullptr)
                entry = &candidate;

        if (entry == nullptr)
            return HilStatus::busy;

        return HilStatus::done;
    }

    HilStatus HilGpioCommands::Find(const HilArguments& arguments, Entry*& entry)
    {
        HilPinId id{};
        HilStatus status = HilStatus::done;
        arguments.PinAt(0, context.naming, id, status);
        if (status != HilStatus::done)
            return status;

        for (auto& candidate : entries)
            if (candidate.id == id)
            {
                entry = &candidate;
                return HilStatus::done;
            }

        return HilStatus::notOpen;
    }

    void HilGpioCommands::Rearm(Entry& entry, std::optional<hal::InterruptTrigger> trigger, hal::InterruptType type) const
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
                    counter->Increment();
                },
                *trigger, type);
            entry.interruptEnabled = true;
        }
    }

    void HilGpioCommands::Free(Entry& entry)
    {
        if (entry.interruptEnabled)
            entry.pin->DisableInterrupt();

        entry.pin->ResetConfig();
        context.pins.Release(*entry.id, HilOwners::gpio);
        entry.id = std::nullopt;
        entry.pin = nullptr;
        entry.interruptEnabled = false;
    }

    void HilGpioCommands::Toggle()
    {
        pulseEntry->pin->Set(!pulseEntry->pin->GetOutputLatch());

        if (!pulseTimer.Armed())
        {
            pulseEntry = nullptr;
            context.response.Ok();
        }
    }
}
