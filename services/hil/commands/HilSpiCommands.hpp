#ifndef SERVICES_HIL_SPI_COMMANDS_HPP
#define SERVICES_HIL_SPI_COMMANDS_HPP

#include "hal/interfaces/Spi.hpp"
#include "hal/synchronous_interfaces/SynchronousSpi.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilSingleInstance.hpp"
#include "services/util/Terminal.hpp"
#include <array>

namespace services
{
    struct HilSpiHandle
    {
        hal::SpiMaster* spi = nullptr;
        hal::SynchronousSpi* synchronous = nullptr;
    };

    class HilSpiFactory
    {
    protected:
        HilSpiFactory() = default;
        HilSpiFactory(const HilSpiFactory& other) = delete;
        HilSpiFactory& operator=(const HilSpiFactory& other) = delete;
        ~HilSpiFactory() = default;

    public:
        virtual uint8_t Instances() const = 0;
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(uint8_t index, const HilArguments& arguments) = 0;
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, HilSpiHandle& handle) = 0;
        virtual void Close(uint8_t index, const infra::Function<void()>& onClosed) = 0;
    };

    class HilSpiCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<HilSpiCommands, std::array<uint8_t, 2 * Capacity>>;

        HilSpiCommands(infra::ByteRange buffers, HilContext& context, HilSpiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus Transfer(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments);
        void StartTransfer(std::size_t transmitSize, std::size_t receiveSize, std::size_t length, bool continueSession);
        void TransferAsynchronous(infra::ConstByteRange send, infra::ByteRange receive, hal::SpiAction nextAction);
        void Done(uint32_t generation);
        void Timeout();
        void Report();
        void Closed();

    private:
        infra::ByteRange transmitBuffer;
        infra::ByteRange receiveBuffer;
        HilContext& context;
        HilSpiFactory& factory;
        HilSingleInstance instance;
        HilPinOwner pins;
        HilSpiHandle handle;
        std::size_t reportSize = 0;
        uint32_t generation = 0;
        bool transferring = false;
        bool awaiting = false;
        infra::TimerSingleShot timer;
        std::array<Command, 3> commands;
    };
}

#endif
