#ifndef SERVICES_GAP_PAIRING_OBSERVER_MOCK_HPP
#define SERVICES_GAP_PAIRING_OBSERVER_MOCK_HPP

#include "services/ble/GapPairing.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapPairingObserverMock
        : public GapPairingObserver
    {
    public:
        using GapPairingObserver::GapPairingObserver;

        MOCK_METHOD(void, DisplayPasskey, (uint32_t passkey), (override));
        MOCK_METHOD(void, ConfirmNumericComparison, (uint32_t value), (override));
        MOCK_METHOD(void, PairingSuccessfullyCompleted, (const GapBondStrength& strength), (override));
        MOCK_METHOD(void, PairingFailed, (GapPairingResult error), (override));
        MOCK_METHOD(void, OutOfBandDataGenerated, (const GapOutOfBandData& outOfBandData), (override));
    };
}

#endif
