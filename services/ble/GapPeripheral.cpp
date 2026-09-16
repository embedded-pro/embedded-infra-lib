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

    infra::ConstByteRange GapPeripheralDecorator::GetAdvertisementData() const
    {
        return GapPeripheralObserver::Subject().GetAdvertisementData();
    }

    infra::ConstByteRange GapPeripheralDecorator::GetScanResponseData() const
    {
        return GapPeripheralObserver::Subject().GetScanResponseData();
    }

    GapRequestStatus GapPeripheralDecorator::SetAdvertisementData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone)
    {
        return GapPeripheralObserver::Subject().SetAdvertisementData(data, onDone);
    }

    GapRequestStatus GapPeripheralDecorator::SetScanResponseData(infra::ConstByteRange data, const infra::Function<void(Result)>& onDone)
    {
        return GapPeripheralObserver::Subject().SetScanResponseData(data, onDone);
    }

    GapRequestStatus GapPeripheralDecorator::Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone)
    {
        return GapPeripheralObserver::Subject().Advertise(type, multiplier, onDone);
    }

    GapRequestStatus GapPeripheralDecorator::Standby(const infra::Function<void(Result)>& onDone)
    {
        return GapPeripheralObserver::Subject().Standby(onDone);
    }

    GapRequestStatus GapPeripheralDecorator::SetConnectionParameters(const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone)
    {
        return GapPeripheralObserver::Subject().SetConnectionParameters(connParam, onDone);
    }
}
