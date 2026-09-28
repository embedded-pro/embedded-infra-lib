#ifndef SERVICES_HIL_ETHERNET_COMMANDS_HPP
#define SERVICES_HIL_ETHERNET_COMMANDS_HPP

#include "hal/interfaces/Ethernet.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/HilCommand.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <optional>

namespace services
{
    class HilEthernetMonitor
        : public hal::EthernetSmiObserver
        , public hal::EthernetMacObserver
    {
    public:
        static constexpr std::size_t frameSize = 1536;

        HilEthernetMonitor(hal::EthernetSmi& smi, hal::EthernetMac& mac, infra::ByteRange receiveStorage);

        void LinkUp(hal::LinkSpeed speed) override;
        void LinkDown() override;

        infra::ByteRange RequestReceiveBuffer() override;
        void ReceivedFrame(uint32_t usedBuffers, uint32_t size) override;
        void ReceivedErrorFrame(uint32_t usedBuffers, uint32_t size) override;
        void SentFrame() override;

        bool Up() const;
        hal::LinkSpeed Speed() const;
        uint32_t Received() const;
        uint32_t Sent() const;

    private:
        infra::ByteRange receiveStorage;
        std::size_t nextBuffer = 0;
        bool up = false;
        hal::LinkSpeed linkSpeed = hal::LinkSpeed::halfDuplex10MHz;
        uint32_t received = 0;
        uint32_t sent = 0;
    };

    struct HilEthernetHandle
    {
        hal::EthernetSmi* smi = nullptr;
        hal::EthernetMac* mac = nullptr;
    };

    class HilEthernetFactory
    {
    protected:
        HilEthernetFactory() = default;
        HilEthernetFactory(const HilEthernetFactory& other) = delete;
        HilEthernetFactory& operator=(const HilEthernetFactory& other) = delete;
        ~HilEthernetFactory() = default;

    public:
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual HilStatus Prepare(const HilArguments& arguments) = 0;
        virtual HilStatus Open(const HilArguments& arguments, HilEthernetHandle& handle) = 0;
        virtual void Close(const infra::Function<void()>& onClosed) = 0;
    };

    class HilEthernetCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t ReceiveBuffers>
        using WithReceiveBuffers = infra::WithStorage<HilEthernetCommands, std::array<uint8_t, ReceiveBuffers * HilEthernetMonitor::frameSize>>;

        HilEthernetCommands(infra::ByteRange receiveStorage, HilContext& context, HilEthernetFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        HilStatus Open(const HilArguments& arguments);
        HilStatus LinkStatus(const HilArguments& arguments);
        HilStatus Close(const HilArguments& arguments);

    private:
        infra::ByteRange receiveStorage;
        HilContext& context;
        HilEthernetFactory& factory;
        std::optional<HilEthernetMonitor> monitor;
        bool closing = false;
        std::array<Command, 3> commands;
    };
}

#endif
