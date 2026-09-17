#ifndef SERVICES_GAP_PAIRING_MOCK_HPP
#define SERVICES_GAP_PAIRING_MOCK_HPP

#include "services/ble/GapPairing.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapPairingMock
        : public GapPairing
    {
    public:
        MOCK_METHOD(GapRequestStatus, PairAndBond, (const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, AllowPairing, (bool allow, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetSecurityMode, (SecurityModeAndLevel modeAndLevel, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetSecureConnectionsOnly, (bool enabled, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetIoCapabilities, (IoCapabilities caps, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, GenerateOutOfBandData, (const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetOutOfBandData, (const GapOutOfBandData& outOfBandData, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, AuthenticateWithPasskey, (uint32_t passkey, const infra::Function<void(GapPairingResult)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, NumericComparisonConfirm, (bool accept, const infra::Function<void(GapPairingResult)>& onDone), (override));
    };
}

#endif
