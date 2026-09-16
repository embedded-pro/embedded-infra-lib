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
        MOCK_METHOD(std::size_t, GetMaxNumberOfBonds, (), (const));
        MOCK_METHOD(std::size_t, GetNumberOfBonds, (), (const));
        MOCK_METHOD(bool, IsDeviceBonded, (hal::MacAddress deviceAddress, GapDeviceAddressType addressType), (const));
        MOCK_METHOD(GapRequestStatus, RemoveAllBonds, (const infra::Function<void()>& onDone));
        MOCK_METHOD(GapRequestStatus, RemoveOldestBond, (const infra::Function<void()>& onDone));
    };
}

#endif
