#ifndef SERVICES_HIL_QEI_COMMANDS_HPP
#define SERVICES_HIL_QEI_COMMANDS_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstanceGroup.hpp"
#include <array>

namespace services
{
    class HilQeiFactory
        : public HilInstanceFactory
    {
    protected:
        HilQeiFactory() = default;
        HilQeiFactory(const HilQeiFactory& other) = delete;
        HilQeiFactory& operator=(const HilQeiFactory& other) = delete;
        ~HilQeiFactory() = default;

    public:
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) = 0;
    };

    class HilQeiCommands
        : public HilSingleInstanceGroup
    {
    public:
        HilQeiCommands(HilContext& context, HilQeiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    protected:
        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) override;
        void CloseInstance() override;

    private:
        HilStatus Read(const HilArguments& arguments);

    private:
        HilQeiFactory& factory;
        hal::SynchronousQuadratureEncoder* encoder = nullptr;
        std::array<Command, 3> commands;
    };
}

#endif
