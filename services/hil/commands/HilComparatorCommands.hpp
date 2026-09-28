#ifndef SERVICES_HIL_COMPARATOR_COMMANDS_HPP
#define SERVICES_HIL_COMPARATOR_COMMANDS_HPP

#include "hal/interfaces/AnalogComparator.hpp"
#include "hal/synchronous_interfaces/SynchronousAnalogComparator.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <atomic>

namespace services
{
    struct HilComparatorHandle
    {
        hal::AnalogComparator* comparator = nullptr;
        hal::SynchronousAnalogComparator* synchronous = nullptr;
    };

    class HilComparatorFactory
    {
    protected:
        HilComparatorFactory() = default;
        HilComparatorFactory(const HilComparatorFactory& other) = delete;
        HilComparatorFactory& operator=(const HilComparatorFactory& other) = delete;
        ~HilComparatorFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, HilComparatorHandle& handle) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class HilComparatorCommands
        : public services::TerminalCommands
    {
    public:
        HilComparatorCommands(HilContext& context, HilComparatorFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Read(const HilArguments& arguments);
        HilStatus Interrupt(const HilArguments& arguments);
        HilStatus Count(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments);
        void Closed();

    private:
        HilContext& context;
        HilComparatorFactory& factory;
        HilSingleInstance instance;
        HilPinOwner pins;
        HilComparatorHandle handle;
        std::atomic<uint32_t> count{ 0 };
        std::array<Command, 5> commands;
    };
}

#endif
