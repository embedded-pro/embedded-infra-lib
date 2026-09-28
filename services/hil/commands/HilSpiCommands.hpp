#ifndef SERVICES_HIL_SPI_COMMANDS_HPP
#define SERVICES_HIL_SPI_COMMANDS_HPP

#include "hal/interfaces/Spi.hpp"
#include "hal/synchronous_interfaces/SynchronousSpi.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/hil/commands/HilPendingOperation.hpp"
#include "services/hil/commands/HilSingleInstanceGroup.hpp"
#include <array>

namespace services
{
    struct HilSpiHandle
    {
        hal::SpiMaster* spi = nullptr;
        hal::SynchronousSpi* synchronous = nullptr;
    };

    class HilSpiFactory
        : public HilInstanceFactory
    {
    protected:
        HilSpiFactory() = default;
        HilSpiFactory(const HilSpiFactory& other) = delete;
        HilSpiFactory& operator=(const HilSpiFactory& other) = delete;
        ~HilSpiFactory() = default;

    public:
        virtual HilStatus Open(uint8_t index, const HilArguments& arguments, HilPinOwner& pins, HilSpiHandle& handle) = 0;
    };

    class HilSpiCommands
        : public HilSingleInstanceGroup
    {
    public:
        template<std::size_t Capacity>
        using WithCapacity = infra::WithStorage<HilSpiCommands, std::array<uint8_t, 2 * Capacity>>;

        HilSpiCommands(infra::ByteRange buffers, HilContext& context, HilSpiFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    protected:
        HilStatus OpenInstance(uint8_t index, const HilArguments& arguments) override;
        void CloseInstance() override;

    private:
        HilStatus Transfer(const HilArguments& arguments);

        void StartTransfer(std::size_t transmitSize, std::size_t receiveSize, std::size_t length, bool continueSession);
        void TransferAsynchronous(infra::ConstByteRange send, infra::ByteRange receive, hal::SpiAction nextAction);
        void Report() const;

    private:
        infra::ByteRange transmitBuffer;
        infra::ByteRange receiveBuffer;
        HilSpiFactory& factory;
        HilSpiHandle handle;
        std::size_t reportSize = 0;
        HilPendingOperation transfer;
        std::array<Command, 3> commands;
    };
}

#endif
