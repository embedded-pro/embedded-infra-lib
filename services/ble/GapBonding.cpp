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

    std::optional<GapBondStrength> GapBondingDecorator::BondStrength(hal::MacAddress address, GapDeviceAddressType addressType) const
    {
        return GapBondingObserver::Subject().BondStrength(address, addressType);
    }

    GapRequestStatus GapBondingDecorator::RemoveAllBonds(const infra::Function<void()>& onDone)
    {
        return GapBondingObserver::Subject().RemoveAllBonds(onDone);
    }

    GapRequestStatus GapBondingDecorator::RemoveOldestBond(const infra::Function<void()>& onDone)
    {
        return GapBondingObserver::Subject().RemoveOldestBond(onDone);
    }
}
