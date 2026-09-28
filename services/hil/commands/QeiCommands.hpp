#ifndef SERVICES_HIL_QEI_COMMANDS_HPP
#define SERVICES_HIL_QEI_COMMANDS_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services::hil
{
    class QeiFactory
    {
    protected:
        QeiFactory() = default;
        QeiFactory(const QeiFactory& other) = delete;
        QeiFactory& operator=(const QeiFactory& other) = delete;
        ~QeiFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t index, const Arguments& arguments, PinOwner& pins, hal::SynchronousQuadratureEncoder*& encoder) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class QeiCommands
        : public services::TerminalCommands
    {
    public:
        QeiCommands(Context& context, QeiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Read(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t index, const Arguments& arguments);
        void Closed();

    private:
        Context& context;
        QeiFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        hal::SynchronousQuadratureEncoder* encoder = nullptr;
        std::array<Command, 3> commands;
    };
}

#endif
