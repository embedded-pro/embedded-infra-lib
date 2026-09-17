#ifndef SERVICES_DIRECT_TEST_MODE_MOCK_HPP
#define SERVICES_DIRECT_TEST_MODE_MOCK_HPP

#include "services/ble/DirectTestMode.hpp"
#include "gmock/gmock.h"

namespace services
{
    class DirectTestModeMock
        : public DirectTestMode
    {
    public:
        MOCK_METHOD(RequestStatus, StartReceiverTest, (uint8_t channel, Phy phy, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(RequestStatus, StartTransmitterTest, (uint8_t channel, uint8_t dataLength, PacketPayload payload, Phy phy, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(RequestStatus, EndTest, (const infra::Function<void(Result, uint16_t packetsReceived)>& onDone), (override));
        MOCK_METHOD(RequestStatus, StartUnmodulatedCarrier, (uint8_t channel, uint8_t offset, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(RequestStatus, StopUnmodulatedCarrier, (const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(RequestStatus, SetTransmitPowerLevel, (int8_t txPower, const infra::Function<void(Result)>& onDone), (override));
    };
}

#endif
