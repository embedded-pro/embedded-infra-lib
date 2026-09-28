#ifndef SERVICES_HIL_GPIO_COMMANDS_HPP
#define SERVICES_HIL_GPIO_COMMANDS_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>
#include <optional>

namespace services::hil
{
    class GpioCommands
        : public services::TerminalCommands
    {
    public:
        struct Entry
        {
            std::optional<PinId> id;
            hal::GpioPin* pin = nullptr;
            bool output = false;
            bool interruptEnabled = false;
            std::atomic<uint32_t> count{ 0 };
        };

        template<std::size_t MaxPins>
        using WithMaxPins = infra::WithStorage<GpioCommands, std::array<Entry, MaxPins>>;

        GpioCommands(infra::MemoryRange<Entry> entries, Context& context);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Configure(const Arguments& arguments);
        Status Set(const Arguments& arguments);
        Status Get(const Arguments& arguments);
        Status Pulse(const Arguments& arguments);
        Status Interrupt(const Arguments& arguments);
        Status Count(const Arguments& arguments);
        Status Release(const Arguments& arguments);

        Status ParseConfiguration(const Arguments& arguments, PinId& id, bool& output, PinOptions& options) const;
        Status Allocate(PinId id, Entry*& entry);
        Status Find(const Arguments& arguments, Entry*& entry);
        void Rearm(Entry& entry, std::optional<hal::InterruptTrigger> trigger, hal::InterruptType type);
        void Free(Entry& entry);
        void Toggle();

    private:
        infra::MemoryRange<Entry> entries;
        Context& context;
        infra::TimerRepeating pulseTimer;
        Entry* pulseEntry = nullptr;
        uint32_t pulsesRemaining = 0;
        std::array<Command, 7> commands;
    };
}

#endif
