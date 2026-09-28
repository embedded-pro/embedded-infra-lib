#ifndef SERVICES_HIL_QEI_COMMANDS_HPP
#define SERVICES_HIL_QEI_COMMANDS_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services
{
    class HilQeiFactory
    {
    protected:
        HilQeiFactory() = default;
        HilQeiFactory(const HilQeiFactory& other) = delete;
        HilQeiFactory& operator=(const HilQeiFactory& other) = delete;
        ~HilQeiFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class HilQeiCommands
        : public services::TerminalCommands
    {
    public:
        HilQeiCommands(HilContext& context, HilQeiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Read(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments);
        void Closed();

    private:
        HilContext& context;
        HilQeiFactory& factory;
        HilSingleInstance instance;
        HilPinOwner pins;
        hal::SynchronousQuadratureEncoder* encoder = nullptr;
        std::array<Command, 3> commands;
    };
}

#endif
