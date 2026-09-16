#include "services/ble/GapCentral.hpp"

namespace services
{
    void GapCentralDecorator::DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered)
    {
        GapCentralObserver::SubjectType::NotifyObservers([&deviceDiscovered](auto& obs)
            {
                obs.DeviceDiscovered(deviceDiscovered);
            });
    }

    void GapCentralDecorator::StateChanged(GapState state)
    {
        GapCentralObserver::SubjectType::NotifyObservers([&state](auto& obs)
            {
                obs.StateChanged(state);
            });
    }

    std::optional<hal::MacAddress> GapCentralDecorator::ResolvePrivateAddress(hal::MacAddress address) const
    {
        return GapCentralObserver::Subject().ResolvePrivateAddress(address);
    }

    GapRequestStatus GapCentralDecorator::Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().Connect(macAddress, addressType, initiatingTimeout, onDone);
    }

    GapRequestStatus GapCentralDecorator::CancelConnect(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().CancelConnect(onDone);
    }

    GapRequestStatus GapCentralDecorator::Disconnect(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().Disconnect(onDone);
    }

    GapRequestStatus GapCentralDecorator::SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().SetAddress(macAddress, addressType, onDone);
    }

    GapRequestStatus GapCentralDecorator::StartDeviceDiscovery(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().StartDeviceDiscovery(onDone);
    }

    GapRequestStatus GapCentralDecorator::StopDeviceDiscovery(const infra::Function<void(Result)>& onDone)
    {
        return GapCentralObserver::Subject().StopDeviceDiscovery(onDone);
    }
}
