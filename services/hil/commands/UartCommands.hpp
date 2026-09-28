#ifndef SERVICES_HIL_UART_COMMANDS_HPP
#define SERVICES_HIL_UART_COMMANDS_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/synchronous_interfaces/SynchronousSerialCommunication.hpp"
#include "hal/synchronous_interfaces/TimeKeeper.hpp"
#include "infra/event/QueueForOneReaderOneIrqWriter.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/DeadlineTimeKeeper.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <optional>

namespace services::hil
{
    struct UartHandle
    {
        hal::SerialCommunication* serial = nullptr;
        hal::SynchronousSerialCommunication* synchronous = nullptr;
        uint32_t baudRate = 0;
    };

    class UartFactory
    {
    protected:
        UartFactory() = default;
        UartFactory(const UartFactory& other) = delete;
        UartFactory& operator=(const UartFactory& other) = delete;
        ~UartFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t index, const Arguments& arguments, PinOwner& pins, hal::TimeKeeper& timeKeeper, UartHandle& handle) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class UartCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t ReceiveCapacity, std::size_t TransmitCapacity>
        using WithCapacity = infra::WithStorage<infra::WithStorage<UartCommands, std::array<uint8_t, ReceiveCapacity + 1>>, std::array<uint8_t, TransmitCapacity>>;

        UartCommands(infra::MemoryRange<uint8_t> receiveStorage, infra::ByteRange transmitBuffer, Context& context, UartFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Send(const Arguments& arguments);
        Status Receive(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t index, const Arguments& arguments);
        void SendAsynchronous(infra::ConstByteRange data);
        void StartReceive(std::optional<std::size_t> length, infra::Duration timeout);
        void Received(infra::ConstByteRange data);
        void DrainSynchronous(std::size_t wanted);
        void CheckReceive();
        void FinishReceive();
        void SendDone(uint32_t generation);
        void SendTimeout();
        void Closed();

    private:
        infra::ByteRange transmitBuffer;
        Context& context;
        UartFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        UartHandle handle;
        DeadlineTimeKeeper timeKeeper;
        infra::QueueForOneReaderOneIrqWriter<uint8_t> received;
        uint32_t sendGeneration = 0;
        bool transmitting = false;
        bool awaitingSend = false;
        std::optional<std::size_t> receiveWanted;
        infra::TimerSingleShot sendTimer;
        infra::TimerSingleShot receiveTimer;
        std::array<Command, 4> commands;
    };
}

#endif
