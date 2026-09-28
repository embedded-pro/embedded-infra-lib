#ifndef SERVICES_HIL_ETHERNET_COMMANDS_HPP
#define SERVICES_HIL_ETHERNET_COMMANDS_HPP

#include "hal/interfaces/Ethernet.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/hil/Command.hpp"
#include "services/util/Terminal.hpp"
#include <array>
#include <optional>

namespace services::hil
{
    class EthernetMonitor
        : public hal::EthernetSmiObserver
        , public hal::EthernetMacObserver
    {
    public:
        static constexpr std::size_t frameSize = 1536;

        EthernetMonitor(hal::EthernetSmi& smi, hal::EthernetMac& mac, infra::ByteRange receiveStorage);

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

    struct EthernetHandle
    {
        hal::EthernetSmi* smi = nullptr;
        hal::EthernetMac* mac = nullptr;
    };

    class EthernetFactory
    {
    protected:
        EthernetFactory() = default;
        EthernetFactory(const EthernetFactory& other) = delete;
        EthernetFactory& operator=(const EthernetFactory& other) = delete;
        ~EthernetFactory() = default;

    public:
        virtual infra::MemoryRange<const char* const> OpenKeys() const = 0;
        virtual Status Prepare(const Arguments& arguments) = 0;
        virtual Status Open(const Arguments& arguments, EthernetHandle& handle) = 0;
        virtual void Close(const infra::Function<void()>& onClosed) = 0;
    };

    class EthernetCommands
        : public services::TerminalCommands
    {
    public:
        template<std::size_t ReceiveBuffers>
        using WithReceiveBuffers = infra::WithStorage<EthernetCommands, std::array<uint8_t, ReceiveBuffers * EthernetMonitor::frameSize>>;

        EthernetCommands(infra::ByteRange receiveStorage, Context& context, EthernetFactory& factory);

        infra::MemoryRange<const Command> Commands() override;

    private:
        Status Open(const Arguments& arguments);
        Status LinkStatus(const Arguments& arguments);
        Status Close(const Arguments& arguments);

    private:
        infra::ByteRange receiveStorage;
        Context& context;
        EthernetFactory& factory;
        std::optional<EthernetMonitor> monitor;
        bool closing = false;
        std::array<Command, 3> commands;
    };
}

#endif
