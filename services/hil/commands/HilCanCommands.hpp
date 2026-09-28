#ifndef SERVICES_HIL_CAN_COMMANDS_HPP
#define SERVICES_HIL_CAN_COMMANDS_HPP

#include "hal/interfaces/Can.hpp"
#include "infra/timer/Timer.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilPendingOperation.hpp"
#include "services/hil/commands/HilSingleInstanceGroup.hpp"
#include <array>

namespace services
{
    class HilCanFactory
        : public HilInstanceFactory
    {
    protected:
        HilCanFactory() = default;
        HilCanFactory(const HilCanFactory& other) = delete;
        HilCanFactory& operator=(const HilCanFactory& other) = delete;
        ~HilCanFactory() = default;

    public:
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, const infra::Function<void(const char* error)>& onError, hal::Can*& can) = 0;
    };

    class HilCanCommands
        : public HilSingleInstanceGroup
    {
    public:
        HilCanCommands(HilContext& context, HilCanFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    protected:
        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) override;
        void CloseInstance() override;

    private:
        HilStatus Send(const HilArguments& arguments);

        void Transmit(hal::Can::Id id, infra::ConstByteRange data);
        void Received(hal::Can::Id id, const hal::Can::Message& data) const;
        void Error(const char* error);

    private:
        HilCanFactory& factory;
        hal::Can* can = nullptr;
        HilPendingOperation sending;
        const char* lastError = nullptr;
        infra::TimePoint lastErrorTime;
        std::array<Command, 3> commands;
    };
}

#endif
