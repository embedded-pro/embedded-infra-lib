#ifndef SERVICES_GAP_PERIPHERAL_MOCK_HPP
#define SERVICES_GAP_PERIPHERAL_MOCK_HPP

#include "services/ble/GapPeripheral.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapPeripheralMock
        : public GapPeripheral
    {
    public:
        MOCK_METHOD(GapAddress, GetAddress, (), (const));
        MOCK_METHOD(GapAddress, GetIdentityAddress, (), (const));
        MOCK_METHOD(infra::ConstByteRange, GetAdvertisementData, (), (const));
        MOCK_METHOD(infra::ConstByteRange, GetScanResponseData, (), (const));
        MOCK_METHOD(GapRequestStatus, SetAdvertisementData, (infra::ConstByteRange data, const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, SetScanResponseData, (infra::ConstByteRange data, const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, Advertise, (GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier, const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, Standby, (const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, SetConnectionParameters, (const GapConnectionParameters& connParam, const infra::Function<void(Result)>& onDone));

        void ChangeState(GapPeripheralState newState)
        {
            NotifyObservers([newState](auto& observer)
                {
                    observer.StateChanged(newState);
                });
        }
    };
}

#endif
