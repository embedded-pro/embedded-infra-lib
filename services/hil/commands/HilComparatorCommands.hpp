#ifndef SERVICES_HIL_COMPARATOR_COMMANDS_HPP
#define SERVICES_HIL_COMPARATOR_COMMANDS_HPP

#include "hal/interfaces/AnalogComparator.hpp"
#include "hal/synchronous_interfaces/SynchronousAnalogComparator.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilEdge.hpp"
#include "services/hil/commands/HilSingleInstanceGroup.hpp"
#include <array>

namespace services
{
    struct HilComparatorHandle
    {
        hal::AnalogComparator* comparator = nullptr;
        hal::SynchronousAnalogComparator* synchronous = nullptr;
    };

    class HilComparatorFactory
        : public HilInstanceFactory
    {
    protected:
        HilComparatorFactory() = default;
        HilComparatorFactory(const HilComparatorFactory& other) = delete;
        HilComparatorFactory& operator=(const HilComparatorFactory& other) = delete;
        ~HilComparatorFactory() = default;

    public:
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, HilComparatorHandle& handle) = 0;
    };

    class HilComparatorCommands
        : public HilSingleInstanceGroup
    {
    public:
        HilComparatorCommands(HilContext& context, HilComparatorFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    protected:
        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) override;
        void CloseInstance() override;

    private:
        HilStatus Read(const HilArguments& arguments);
        HilStatus Interrupt(const HilArguments& arguments);
        HilStatus Count(const HilArguments& arguments);

    private:
        HilComparatorFactory& factory;
        HilComparatorHandle handle;
        HilEdgeCounter count;
        std::array<Command, 5> commands;
    };
}

#endif
