#ifndef SERVICES_HIL_GPIO_COMMANDS_HPP
#define SERVICES_HIL_GPIO_COMMANDS_HPP

#include "infra/timer/TimerLimitedRepeating.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilEdge.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <optional>

namespace services
{
    class HilGpioCommands
        : public services::TerminalCommands
    {
    public:
        struct Entry
        {
            std::optional<HilPinId> id;
            hal::GpioPin* pin = nullptr;
            bool output = false;
            bool interruptEnabled = false;
            HilEdgeCounter count;
        };

        template<std::size_t MaxPins>
        using WithMaxPins = infra::WithStorage<HilGpioCommands, std::array<Entry, MaxPins>>;

        HilGpioCommands(infra::MemoryRange<Entry> entries, HilContext& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Configure(const HilArguments& arguments);
        HilStatus Set(const HilArguments& arguments);
        HilStatus Get(const HilArguments& arguments);
        HilStatus Pulse(const HilArguments& arguments);
        HilStatus Interrupt(const HilArguments& arguments);
        HilStatus Count(const HilArguments& arguments);
        HilStatus Release(const HilArguments& arguments);

        HilStatus ParseConfiguration(const HilArguments& arguments, HilPinId& id, bool& output, HilPinOptions& options) const;
        HilStatus Allocate(HilPinId id, Entry*& entry);
        HilStatus Find(const HilArguments& arguments, Entry*& entry);
        void Rearm(Entry& entry, std::optional<hal::InterruptTrigger> trigger, hal::InterruptType type) const;
        void Free(Entry& entry);
        void Toggle();

    private:
        infra::MemoryRange<Entry> entries;
        HilContext& context;
        infra::TimerLimitedRepeating pulseTimer;
        Entry* pulseEntry = nullptr;
        std::array<Command, 7> commands;
    };
}

#endif
