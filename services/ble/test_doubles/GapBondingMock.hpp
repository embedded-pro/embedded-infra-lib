#ifndef SERVICES_GAP_BONDING_MOCK_HPP
#define SERVICES_GAP_BONDING_MOCK_HPP

#include "services/ble/GapBonding.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapBondingMock
        : public GapBonding
    {
    public:
        MOCK_METHOD(std::size_t, GetMaxNumberOfBonds, (), (const, override));
        MOCK_METHOD(std::size_t, GetNumberOfBonds, (), (const, override));
        MOCK_METHOD(bool, IsDeviceBonded, (const GapAddress& address), (const, override));
        MOCK_METHOD(std::optional<GapBondStrength>, BondStrength, (const GapAddress& address), (const, override));
        MOCK_METHOD(GapRequestStatus, RemoveAllBonds, (const infra::Function<void()>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, RemoveOldestBond, (const infra::Function<void()>& onDone), (override));
    };
}

#endif
