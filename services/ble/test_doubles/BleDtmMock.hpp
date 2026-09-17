#ifndef SERVICES_BLE_DTM_MOCK_HPP
#define SERVICES_BLE_DTM_MOCK_HPP

#include "services/ble/BleDtm.hpp"
#include "gmock/gmock.h"

namespace services
{
    class BleDtmMock
        : public BleDtm
    {
    public:
        MOCK_METHOD(DtmRequestStatus, StartReceiverTest, (uint8_t channel, DtmPhy phy, const infra::Function<void(DtmResult)>& onDone), (override));
        MOCK_METHOD(DtmRequestStatus, StartTransmitterTest, (uint8_t channel, uint8_t dataLength, DtmPacketPayload payload, DtmPhy phy, const infra::Function<void(DtmResult)>& onDone), (override));
        MOCK_METHOD(DtmRequestStatus, EndTest, (const infra::Function<void(DtmResult, uint16_t packetsReceived)>& onDone), (override));
        MOCK_METHOD(DtmRequestStatus, StartUnmodulatedCarrier, (uint8_t channel, uint8_t offset, const infra::Function<void(DtmResult)>& onDone), (override));
        MOCK_METHOD(DtmRequestStatus, StopUnmodulatedCarrier, (const infra::Function<void(DtmResult)>& onDone), (override));
        MOCK_METHOD(DtmRequestStatus, SetTransmitPowerLevel, (int8_t txPower, const infra::Function<void(DtmResult)>& onDone), (override));
    };
}

#endif
