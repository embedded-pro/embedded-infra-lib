#ifndef SERVICES_GAP_CENTRAL_HPP
#define SERVICES_GAP_CENTRAL_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/Function.hpp"
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
        enum class Result : uint8_t
        {
            success = 0,
            cancelled,
            timeout,
            connectionFailed,
            controllerError
        };

        virtual std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const = 0;

        // Each procedure below reports whether the request is accepted through its
        // return value, and its outcome through onDone. onDone is never invoked from
        // within the call itself; it is scheduled on the event dispatcher. A request
        // that is not accepted never results in a call to onDone.
        virtual GapRequestStatus Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone) = 0;
        virtual GapRequestStatus StartDeviceDiscovery(const infra::Function<void(Result)>& onDone) = 0;
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
        void StateChanged(GapState state) override;

        // Implementation of GapCentral
        std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const override;
        GapRequestStatus Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus CancelConnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus Disconnect(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus StartDeviceDiscovery(const infra::Function<void(Result)>& onDone) override;
        GapRequestStatus StopDeviceDiscovery(const infra::Function<void(Result)>& onDone) override;
    };
}

#endif
