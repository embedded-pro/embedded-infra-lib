#ifndef SERVICES_PERSISTENT_BOND_STORAGE_HPP
#define SERVICES_PERSISTENT_BOND_STORAGE_HPP

#include "services/ble/VolatileBondStorage.hpp"
#include "services/util/ConfigurationStore.hpp"

namespace services
{
    class PersistentBondStorage
        : public VolatileBondStorage
    {
    public:
        template<std::size_t MaxBonds>
        using WithMaxBonds = infra::WithStorage<PersistentBondStorage, infra::BoundedVector<hal::MacAddress>::WithMaxSize<MaxBonds>>;

        PersistentBondStorage(infra::BoundedVector<hal::MacAddress>& addresses, ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage);

        void UpdateBondedDevice(hal::MacAddress address) override;
        void RemoveBond(hal::MacAddress address) override;
        void RemoveAllBonds() override;
        void RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress) override;

    private:
        void Persist();

        ConfigurationStoreAccess<infra::BoundedVector<uint8_t>> storage;
    };
}

#endif
