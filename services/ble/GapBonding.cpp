#include "services/ble/GapBonding.hpp"

namespace services
{
    void GapBondingDecorator::NumberOfBondsChanged(std::size_t nrBonds)
    {
        GapBonding::NotifyObservers([&nrBonds](auto& obs)
            {
                obs.NumberOfBondsChanged(nrBonds);
            });
    }

    void GapBondingDecorator::RemoveAllBonds()
    {
        GapBondingObserver::Subject().RemoveAllBonds();
    }

    void GapBondingDecorator::RemoveOldestBond()
    {
        GapBondingObserver::Subject().RemoveOldestBond();
    }

    std::size_t GapBondingDecorator::GetMaxNumberOfBonds() const
    {
        return GapBondingObserver::Subject().GetMaxNumberOfBonds();
    }

    std::size_t GapBondingDecorator::GetNumberOfBonds() const
    {
        return GapBondingObserver::Subject().GetNumberOfBonds();
    }

    bool GapBondingDecorator::IsDeviceBonded(hal::MacAddress address, GapDeviceAddressType addressType) const
    {
        return GapBondingObserver::Subject().IsDeviceBonded(address, addressType);
    }
}
