#ifndef SERVICES_HIL_COMPARATOR_COMMANDS_HPP
#define SERVICES_HIL_COMPARATOR_COMMANDS_HPP

#include "hal/interfaces/AnalogComparator.hpp"
#include "hal/synchronous_interfaces/SynchronousAnalogComparator.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>

namespace services::hil
{
    struct ComparatorHandle
    {
        hal::AnalogComparator* comparator = nullptr;
        hal::SynchronousAnalogComparator* synchronous = nullptr;
    };

    class ComparatorFactory
    {
    protected:
        ComparatorFactory() = default;
        ComparatorFactory(const ComparatorFactory& other) = delete;
        ComparatorFactory& operator=(const ComparatorFactory& other) = delete;
        ~ComparatorFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t index, const Arguments& arguments, PinOwner& pins, ComparatorHandle& handle) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class ComparatorCommands
        : public services::TerminalCommands
    {
    public:
        ComparatorCommands(Context& context, ComparatorFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Interrupt(const Arguments& arguments);
        Status Count(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t index, const Arguments& arguments);
        void Closed();

    private:
        Context& context;
        ComparatorFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        ComparatorHandle handle;
        std::atomic<uint32_t> count{ 0 };
        std::array<Command, 5> commands;
    };
}

#endif
