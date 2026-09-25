#ifndef SERVICES_VOLATILE_BOND_STORAGE_HPP
#define SERVICES_VOLATILE_BOND_STORAGE_HPP

#include "infra/util/BoundedVector.hpp"
#include "infra/util/WithStorage.hpp"
#include "services/ble/BondStorageSynchronizer.hpp"

namespace services
{
    class VolatileBondStorage
        : public BondStorage
    {
    public:
        template<std::size_t MaxBonds>
        using WithMaxBonds = infra::WithStorage<VolatileBondStorage, infra::BoundedVector<hal::MacAddress>::WithMaxSize<MaxBonds>>;

        explicit VolatileBondStorage(infra::BoundedVector<hal::MacAddress>& addresses);

        void BondStorageSynchronizerCreated(BondStorageSynchronizer& manager) override;
        void UpdateBondedDevice(hal::MacAddress address) override;
        void RemoveBond(hal::MacAddress address) override;
        void RemoveAllBonds() override;
        void RemoveBondIf(const infra::Function<bool(hal::MacAddress)>& onAddress) override;
        uint32_t GetMaxNumberOfBonds() const override;
        bool IsBondStored(hal::MacAddress address) const override;
        void IterateBondedDevices(const infra::Function<void(hal::MacAddress)>& onAddress) override;

    private:
        infra::BoundedVector<hal::MacAddress>& addresses;
    };
}

#endif
