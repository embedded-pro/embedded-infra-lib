#include "services/ble/PersistentBondStorage.hpp"
#include <algorithm>
#include <tuple>

namespace services
{
    PersistentBondStorage::PersistentBondStorage(infra::BoundedVector<hal::MacAddress>& addresses, ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage)
        : VolatileBondStorage(addresses)
        , storage(storage)
    {
        const auto& stored = *this->storage;

        for (std::size_t offset = 0; offset + std::tuple_size_v<hal::MacAddress> <= stored.size(); offset += std::tuple_size_v<hal::MacAddress>)
        {
            hal::MacAddress address{};
            std::copy(stored.begin() + offset, stored.begin() + offset + address.size(), address.begin());
            VolatileBondStorage::UpdateBondedDevice(address);
        }
    }

    void PersistentBondStorage::UpdateBondedDevice(hal::MacAddress address)
    {
        VolatileBondStorage::UpdateBondedDevice(address);
        Persist();
    }

    void PersistentBondStorage::RemoveBond(hal::MacAddress address)
    {
        VolatileBondStorage::RemoveBond(address);
        Persist();
    }

    void PersistentBondStorage::RemoveAllBonds()
    {
        VolatileBondStorage::RemoveAllBonds();
        Persist();
    }

    void PersistentBondStorage::RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress)
    {
        VolatileBondStorage::RemoveBondIf(onAddress);
        Persist();
    }

    void PersistentBondStorage::Persist()
    {
        auto& stored = *storage;
        stored.clear();

        IterateBondedDevices([&stored](hal::MacAddress address)
            {
                stored.insert(stored.end(), address.begin(), address.end());
            });

        storage.Write();
    }
}
