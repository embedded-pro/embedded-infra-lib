#ifndef SERVICES_HIL_CAN_COMMANDS_HPP
#define SERVICES_HIL_CAN_COMMANDS_HPP

#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services::hil
{
    class CanFactory
    {
    protected:
        CanFactory() = default;
        CanFactory(const CanFactory& other) = delete;
        CanFactory& operator=(const CanFactory& other) = delete;
        ~CanFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t index, const Arguments& arguments, PinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class CanCommands
        : public services::TerminalCommands
    {
    public:
        CanCommands(Context& context, CanFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Send(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t index, const Arguments& arguments);
        void Transmit(hal::Can::Id id, infra::ConstByteRange data);
        void Received(hal::Can::Id id, const hal::Can::Message& data);
        void Error(const char* error);
        void SendDone(uint32_t generation, bool success);
        void SendTimeout();
        void Closed();

    private:
        Context& context;
        CanFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        hal::Can* can = nullptr;
        uint32_t generation = 0;
        bool transmitting = false;
        bool awaiting = false;
        const char* lastError = nullptr;
        infra::TimePoint lastErrorTime;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
