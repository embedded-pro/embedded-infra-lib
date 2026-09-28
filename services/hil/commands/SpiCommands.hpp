#ifndef SERVICES_HIL_SPI_COMMANDS_HPP
#define SERVICES_HIL_SPI_COMMANDS_HPP

#include "hal/interfaces/Spi.hpp"
#include "hal/synchronous_interfaces/SynchronousSpi.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/hil/commands/SingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services::hil
{
    struct SpiHandle
    {
        hal::SpiMaster* spi = nullptr;
        hal::SynchronousSpi* synchronous = nullptr;
    };

    class SpiFactory
    {
    protected:
        SpiFactory() = default;
        SpiFactory(const SpiFactory& other) = delete;
        SpiFactory& operator=(const SpiFactory& other) = delete;
        ~SpiFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(uint8_t index, const Arguments& arguments) = 0;
        virtual Status Open(uint8_t index, const Arguments& arguments, PinOwner& pins, SpiHandle& handle) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class SpiCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<SpiCommands, std::array<uint8_t, 2 * Capacity>>;

        SpiCommands(infra::ByteRange buffers, Context& context, SpiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status Transfer(const Arguments& arguments);
        Status Close(const Arguments& arguments);

        Status OpenInstance(uint8_t index, const Arguments& arguments);
        void StartTransfer(std::size_t transmitSize, std::size_t receiveSize, std::size_t length, bool continueSession);
        void TransferAsynchronous(infra::ConstByteRange send, infra::ByteRange receive, hal::SpiAction nextAction);
        void Done(uint32_t generation);
        void Timeout();
        void Report();
        void Closed();

    private:
        infra::ByteRange transmitBuffer;
        infra::ByteRange receiveBuffer;
        Context& context;
        SpiFactory& factory;
        SingleInstance instance;
        PinOwner pins;
        SpiHandle handle;
        std::size_t reportSize = 0;
        uint32_t generation = 0;
        bool transferring = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
