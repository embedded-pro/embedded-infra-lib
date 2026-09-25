#include "services/ble/PersistentBondStorage.hpp"
#include <algorithm>

namespace services
{
    PersistentBondStorage::PersistentBondStorage(ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage)
        : storage(storage)
    {
        this->storage->resize(NumberOfBonds() * addressSize);
    }

    void PersistentBondStorage::BondStorageSynchronizerCreated(BondStorageSynchronizer&)
    {}

    void PersistentBondStorage::UpdateBondedDevice(hal::MacAddress address)
    {
        if (IsBondStored(address))
            return;

        if (NumberOfBonds() == GetMaxNumberOfBonds())
            EraseAt(0);

        storage->insert(storage->end(), address.begin(), address.end());
        storage.Write();
    }

    void PersistentBondStorage::RemoveBond(hal::MacAddress address)
    {
        RemoveBondIf([&address](hal::MacAddress stored)
            {
                return stored == address;
            });
    }

    void PersistentBondStorage::RemoveAllBonds()
    {
        storage->clear();
        storage.Write();
    }

    void PersistentBondStorage::RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress)
    {
        std::size_t index = 0;

        while (index != NumberOfBonds())
            if (onAddress(AddressAt(index)))
                EraseAt(index);
            else
                ++index;

        storage.Write();
    }

    uint32_t PersistentBondStorage::GetMaxNumberOfBonds() const
    {
        return static_cast<uint32_t>(storage->max_size() / addressSize);
    }

    bool PersistentBondStorage::IsBondStored(hal::MacAddress address) const
    {
        for (std::size_t index = 0; index != NumberOfBonds(); ++index)
            if (AddressAt(index) == address)
                return true;

        return false;
    }

    void PersistentBondStorage::IterateBondedDevices(const infra::Function<void(hal::MacAddress)>& onAddress)
    {
        for (std::size_t index = 0; index != NumberOfBonds(); ++index)
            onAddress(AddressAt(index));
    }

    std::size_t PersistentBondStorage::NumberOfBonds() const
    {
        return storage->size() / addressSize;
    }

    hal::MacAddress PersistentBondStorage::AddressAt(std::size_t index) const
    {
        hal::MacAddress address{};
        const auto first = storage->begin() + index * addressSize;
        std::copy(first, first + addressSize, address.begin());
        return address;
    }

    void PersistentBondStorage::EraseAt(std::size_t index)
    {
        const auto first = storage->begin() + index * addressSize;
        storage->erase(first, first + addressSize);
    }
}
