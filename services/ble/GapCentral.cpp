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

    void GapCentralDecorator::Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout)
    {
        GapCentralObserver::Subject().Connect(macAddress, addressType, initiatingTimeout);
    }

    void GapCentralDecorator::CancelConnect()
    {
        GapCentralObserver::Subject().CancelConnect();
    }

    void GapCentralDecorator::Disconnect()
    {
        GapCentralObserver::Subject().Disconnect();
    }

    void GapCentralDecorator::SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType)
    {
        GapCentralObserver::Subject().SetAddress(macAddress, addressType);
    }

    void GapCentralDecorator::StartDeviceDiscovery()
    {
        GapCentralObserver::Subject().StartDeviceDiscovery();
    }

    void GapCentralDecorator::StopDeviceDiscovery()
    {
        GapCentralObserver::Subject().StopDeviceDiscovery();
    }

    std::optional<hal::MacAddress> GapCentralDecorator::ResolvePrivateAddress(hal::MacAddress address) const
    {
        return GapCentralObserver::Subject().ResolvePrivateAddress(address);
    }
}
