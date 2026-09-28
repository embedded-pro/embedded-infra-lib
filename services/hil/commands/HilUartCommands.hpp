#ifndef SERVICES_HIL_UART_COMMANDS_HPP
#define SERVICES_HIL_UART_COMMANDS_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousSerialCommunication.hpp"
#include "hal/synchronous_interfaces/TimeKeeper.hpp"
#include "infra/event/QueueForOneReaderOneIrqWriter.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/HilDeadlineTimeKeeper.hpp"
#include "services/hil/commands/HilPendingOperation.hpp"
#include "services/hil/commands/HilSingleInstanceGroup.hpp"
#include <array>
#include <optional>

namespace services
{
    struct HilUartHandle
    {
        hal::SerialCommunication* serial = nullptr;
        hal::SynchronousSerialCommunication* synchronous = nullptr;
        uint32_t baudRate = 0;
    };

    class HilUartFactory
        : public HilInstanceFactory
    {
    protected:
        HilUartFactory() = default;
        HilUartFactory(const HilUartFactory& other) = delete;
        HilUartFactory& operator=(const HilUartFactory& other) = delete;
        ~HilUartFactory() = default;

    public:
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, hal::TimeKeeper& timeKeeper, HilUartHandle& handle) = 0;
    };

    class HilUartCommands
        : public HilSingleInstanceGroup
    {
    public:
        template<std::size_t ReceiveCapacity, std::size_t TransmitCapacity>
        using WithCapacity = infra::WithStorage<infra::WithStorage<HilUartCommands, std::array<uint8_t, ReceiveCapacity + 1>>, std::array<uint8_t, TransmitCapacity>>;

        HilUartCommands(infra::MemoryRange<uint8_t> receiveStorage, infra::ByteRange transmitBuffer, HilContext& context, HilUartFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    protected:
        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) override;
        void CloseInstance() override;

    private:
        HilStatus Send(const HilArguments& arguments);
        HilStatus Receive(const HilArguments& arguments);

        void SendAsynchronous(infra::ConstByteRange data);
        void StartReceive(std::optional<std::size_t> length, infra::Duration timeout);
        void Received(infra::ConstByteRange data);
        void DrainSynchronous(std::size_t wanted);
        void CheckReceive();
        void FinishReceive();

    private:
        infra::ByteRange transmitBuffer;
        HilUartFactory& factory;
        HilUartHandle handle;
        HilDeadlineTimeKeeper timeKeeper;
        infra::QueueForOneReaderOneIrqWriter<uint8_t> received;
        HilPendingOperation sending;
        std::optional<std::size_t> receiveWanted;
        infra::TimerSingleShot receiveTimer;
        std::array<Command, 4> commands;
    };
}

#endif
