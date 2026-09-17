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

        virtual std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const = 0;

        virtual GapRequestStatus Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone) = 0;
        // 10 ms of every 10 ms, actively scanning: the abstraction's one documented default,
        // rather than each port inventing its own.
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

        // Implementation of GapCentral
        std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const override;
        GapRequestStatus Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone) override;
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
