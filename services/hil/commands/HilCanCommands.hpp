#ifndef SERVICES_HIL_CAN_COMMANDS_HPP
#define SERVICES_HIL_CAN_COMMANDS_HPP

#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services
{
    class HilCanFactory
    {
    protected:
        HilCanFactory() = default;
        HilCanFactory(const HilCanFactory& other) = delete;
        HilCanFactory& operator=(const HilCanFactory& other) = delete;
        ~HilCanFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class HilCanCommands
        : public services::TerminalCommands
    {
    public:
        HilCanCommands(HilContext& context, HilCanFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Send(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments);
        void Transmit(hal::Can::Id id, infra::ConstByteRange data);
        void Received(hal::Can::Id id, const hal::Can::Message& data);
        void Error(const char* error);
        void SendDone(uint32_t generation, bool success);
        void SendTimeout();
        void Closed();

    private:
        HilContext& context;
        HilCanFactory& factory;
        HilSingleInstance instance;
        HilPinOwner pins;
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
