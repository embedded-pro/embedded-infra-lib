#ifndef SERVICES_GAP_CENTRAL_HPP
#define SERVICES_GAP_CENTRAL_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"
#include <optional>

namespace services
{
    enum class GapCentralState : uint8_t
    {
        standby,
        scanning,
        initiating,
        connected
    };

    class GapCentral;

    class GapCentralObserver
        : public infra::Observer<GapCentralObserver, GapCentral>
    {
    public:
        using infra::Observer<GapCentralObserver, GapCentral>::Observer;

        virtual void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) = 0;
        virtual void StateChanged(GapCentralState state) = 0;

        // Reported whenever the link layer settles on new values, whichever side asked for them.
        virtual void PhyUpdated(GapPhy txPhy, GapPhy rxPhy) = 0;
        virtual void DataLengthChanged(const GapDataLength& dataLength) = 0;
    };

    class GapCentral
        : public infra::Subject<GapCentralObserver>
    {
    public:
        enum class Result : uint8_t
        {
            success = 0,
            cancelled,
            timeout,
            connectionFailed,
            controllerError
        };

        // An identity address is public or static random. A resolvable private address is by
        // definition random, so the parameter carries no type.
        virtual std::optional<GapAddress> ResolvePrivateAddress(hal::MacAddress address) const = 0;

        // The connection parameters travel in CONNECT_IND, so the initiator is what chooses them.
        // A peripheral can only ask for them to be changed afterwards.
        // Bluetooth Core Specification, Volume 6, Part B, section 2.3.3.1
        static constexpr GapConnectionParameters defaultConnectionParameters{ 0x0018u, 0x0028u, 0u, 0x01F4u };

        virtual GapRequestStatus Connect(const GapAddress& peer, const GapConnectionParameters& parameters, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) = 0;
        GapRequestStatus Connect(const GapAddress& peer, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone);

        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.18
        virtual GapRequestStatus UpdateConnectionParameters(const GapConnectionParameters& parameters, const infra::Function<void(Result)>& onDone) = 0;

        virtual GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetAddress(const GapAddress& address, const infra::Function<void(Result)>& onDone) = 0;

        // What the link layer settles on is reported to every observer through DataLengthChanged
        // and PhyUpdated, so neither request carries a completion of its own.
        // Bluetooth Core Specification, Volume 4, Part E, sections 7.8.33 and 7.8.49
        virtual GapRequestStatus SetDataLength(const GapDataLength& dataLength) = 0;
        virtual GapRequestStatus SetPhy(GapPhy txPhy, GapPhy rxPhy) = 0;

        static constexpr GapScanParameters defaultScanParameters{ 0x0010u, 0x0010u, GapScanType::active };

        virtual GapRequestStatus StartDeviceDiscovery(const GapScanParameters& parameters, const infra::Function<void(Result)>& onDone) = 0;
        GapRequestStatus StartDeviceDiscovery(const infra::Function<void(Result)>& onDone);
        virtual GapRequestStatus StopDeviceDiscovery(const infra::Function<void(Result)>& onDone) = 0;
    };

    class GapCentralDecorator
        : public GapCentralObserver
        , public GapCentral
    {
    public:
        using GapCentralObserver::GapCentralObserver;

        // Implementation of GapCentralObserver
        void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) override;
        void StateChanged(GapCentralState state) override;
        void PhyUpdated(GapPhy txPhy, GapPhy rxPhy) override;
        void DataLengthChanged(const GapDataLength& dataLength) override;

        // Implementation of GapCentral
        std::optional<GapAddress> ResolvePrivateAddress(hal::MacAddress address) const override;
        using GapCentral::Connect;
        GapRequestStatus Connect(const GapAddress& peer, const GapConnectionParameters& parameters, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus UpdateConnectionParameters(const GapConnectionParameters& parameters, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetAddress(const GapAddress& address, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetDataLength(const GapDataLength& dataLength) override;
        GapRequestStatus SetPhy(GapPhy txPhy, GapPhy rxPhy) override;
        using GapCentral::StartDeviceDiscovery;
        GapRequestStatus StartDeviceDiscovery(const GapScanParameters& parameters, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus StopDeviceDiscovery(const infra::Function<void(Result)>& onDone) override;
    };
}

namespace infra
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const services::GapCentralState& state);
}

#endif
