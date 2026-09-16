#ifndef SERVICES_GAP_BONDING_HPP
#define SERVICES_GAP_BONDING_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    class GapBonding;

    class GapBondingObserver
        : public infra::Observer<GapBondingObserver, GapBonding>
    {
    public:
        using infra::Observer<GapBondingObserver, GapBonding>::Observer;

        virtual void NumberOfBondsChanged(std::size_t nrBonds) = 0;
    };

    class GapBonding
        : public infra::Subject<GapBondingObserver>
    {
    public:
        virtual std::size_t GetMaxNumberOfBonds() const = 0;
        virtual std::size_t GetNumberOfBonds() const = 0;
        virtual bool IsDeviceBonded(hal::MacAddress address, GapDeviceAddressType addressType) const = 0;

        // Each procedure below reports whether the request is accepted through its
        // return value, and its completion through onDone. onDone is never invoked
        // from within the call itself; it is scheduled on the event dispatcher. A
        // request that is not accepted never results in a call to onDone. The
        // resulting number of bonds is reported through the observer.
        virtual GapRequestStatus RemoveAllBonds(const infra::Function<void()>& onDone) = 0;
        virtual GapRequestStatus RemoveOldestBond(const infra::Function<void()>& onDone) = 0;
    };

    class GapBondingDecorator
        : public GapBondingObserver
        , public GapBonding
    {
    public:
        using GapBondingObserver::GapBondingObserver;

        // Implementation of GapBondingObserver
        void NumberOfBondsChanged(std::size_t nrBonds) override;

        // Implementation of GapBonding
        std::size_t GetMaxNumberOfBonds() const override;
        std::size_t GetNumberOfBonds() const override;
        bool IsDeviceBonded(hal::MacAddress address, GapDeviceAddressType addressType) const override;
        GapRequestStatus RemoveAllBonds(const infra::Function<void()>& onDone) override;
        GapRequestStatus RemoveOldestBond(const infra::Function<void()>& onDone) override;
    };
}

#endif
