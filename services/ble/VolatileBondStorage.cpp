#include "services/ble/VolatileBondStorage.hpp"
#include <algorithm>

namespace services
{
    VolatileBondStorage::VolatileBondStorage(infra::BoundedVector<hal::MacAddress>& addresses)
        : addresses(addresses)
    {}

    void VolatileBondStorage::BondStorageSynchronizerCreated(BondStorageSynchronizer&)
    {}

    void VolatileBondStorage::UpdateBondedDevice(hal::MacAddress address)
    {
        if (IsBondStored(address))
            return;

        if (addresses.full())
            addresses.erase(addresses.begin());

        addresses.push_back(address);
    }

    void VolatileBondStorage::RemoveBond(hal::MacAddress address)
    {
        const auto removed = std::ranges::remove(addresses, address);
        addresses.erase(removed.begin(), removed.end());
    }

    void VolatileBondStorage::RemoveAllBonds()
    {
        addresses.clear();
    }

    void VolatileBondStorage::RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress)
    {
        const auto removed = std::ranges::remove_if(addresses, [&onAddress](const auto& address)
            {
                return onAddress(address);
            });
        addresses.erase(removed.begin(), removed.end());
    }

    uint32_t VolatileBondStorage::GetMaxNumberOfBonds() const
    {
        return static_cast<uint32_t>(addresses.max_size());
    }

    bool VolatileBondStorage::IsBondStored(hal::MacAddress address) const
    {
        return std::ranges::find(addresses, address) != addresses.end();
    }

    void VolatileBondStorage::IterateBondedDevices(const infra::Function<void(hal::MacAddress)>& onAddress)
    {
        for (const auto& address : addresses)
            onAddress(address);
    }
}
