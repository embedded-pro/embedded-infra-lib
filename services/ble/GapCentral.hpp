#ifndef SERVICES_GAP_CENTRAL_HPP
#define SERVICES_GAP_CENTRAL_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"
#include <optional>

namespace services
{
    class GapCentral;

    class GapCentralObserver
        : public infra::Observer<GapCentralObserver, GapCentral>
    {
    public:
        using infra::Observer<GapCentralObserver, GapCentral>::Observer;

        virtual void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) = 0;
        virtual void StateChanged(GapState state) = 0;
    };

    class GapCentral
        : public infra::Subject<GapCentralObserver>
    {
    public:
        virtual void Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout) = 0;
        virtual void CancelConnect() = 0;
        virtual void Disconnect() = 0;
        virtual void SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType) = 0;
        virtual void StartDeviceDiscovery() = 0;
        virtual void StopDeviceDiscovery() = 0;
        virtual std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const = 0;
    };

    class GapCentralDecorator
        : public GapCentralObserver
        , public GapCentral
    {
    public:
        using GapCentralObserver::GapCentralObserver;

        // Implementation of GapCentralObserver
        void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) override;
        void StateChanged(GapState state) override;

        // Implementation of GapCentral
        void Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout) override;
        void CancelConnect() override;
        void Disconnect() override;
        void SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType) override;
        void StartDeviceDiscovery() override;
        void StopDeviceDiscovery() override;
        std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const override;
    };
}

#endif
