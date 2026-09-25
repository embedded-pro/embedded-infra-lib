#ifndef SERVICES_PERSISTENT_BOND_STORAGE_HPP
#define SERVICES_PERSISTENT_BOND_STORAGE_HPP

#include "infra/util/BoundedVector.hpp"
#include "services/ble/BondStorageSynchronizer.hpp"
#include "services/util/ConfigurationStore.hpp"
#include <cstddef>
#include <tuple>

namespace services
{
    class PersistentBondStorage
        : public BondStorage
    {
    public:
        explicit PersistentBondStorage(ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage);

        void BondStorageSynchronizerCreated(BondStorageSynchronizer& manager) override;
        void UpdateBondedDevice(hal::MacAddress address) override;
        void RemoveBond(hal::MacAddress address) override;
        void RemoveAllBonds() override;
        void RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress) override;
        uint32_t GetMaxNumberOfBonds() const override;
        bool IsBondStored(hal::MacAddress address) const override;
        void IterateBondedDevices(const infra::Function<void(hal::MacAddress)>& onAddress) override;

    private:
        static constexpr std::size_t addressSize = std::tuple_size_v<hal::MacAddress>;

        std::size_t NumberOfBonds() const;
        hal::MacAddress AddressAt(std::size_t index) const;
        void EraseAt(std::size_t index);

        ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage;
    };
}

#endif
