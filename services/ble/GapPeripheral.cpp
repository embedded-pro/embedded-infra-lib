#include "services/ble/GapPeripheral.hpp"

namespace services
{
    void GapPeripheralDecorator::StateChanged(GapState state)
    {
        GapPeripheral::NotifyObservers([&state](auto& obs)
            {
                obs.StateChanged(state);
            });
    }

    GapAddress GapPeripheralDecorator::GetAddress() const
    {
        return GapPeripheralObserver::Subject().GetAddress();
    }

    GapAddress GapPeripheralDecorator::GetIdentityAddress() const
    {
        return GapPeripheralObserver::Subject().GetIdentityAddress();
    }

    void GapPeripheralDecorator::SetAdvertisementData(infra::ConstByteRange data)
    {
        GapPeripheralObserver::Subject().SetAdvertisementData(data);
    }

    infra::ConstByteRange GapPeripheralDecorator::GetAdvertisementData() const
    {
        return GapPeripheralObserver::Subject().GetAdvertisementData();
    }

    void GapPeripheralDecorator::SetScanResponseData(infra::ConstByteRange data)
    {
        GapPeripheralObserver::Subject().SetScanResponseData(data);
    }

    infra::ConstByteRange GapPeripheralDecorator::GetScanResponseData() const
    {
        return GapPeripheralObserver::Subject().GetScanResponseData();
    }

    void GapPeripheralDecorator::Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier)
    {
        GapPeripheralObserver::Subject().Advertise(type, multiplier);
    }

    void GapPeripheralDecorator::Standby()
    {
        GapPeripheralObserver::Subject().Standby();
    }

    void GapPeripheralDecorator::SetConnectionParameters(const GapConnectionParameters& connParam)
    {
        GapPeripheralObserver::Subject().SetConnectionParameters(connParam);
    }
}
