#include "services/ble/GapPairing.hpp"

namespace services
{
    void GapPairingDecorator::DisplayPasskey(int32_t passkey, bool numericComparison)
    {
        GapPairing::NotifyObservers([&passkey, &numericComparison](auto& obs)
            {
                obs.DisplayPasskey(passkey, numericComparison);
            });
    }

    void GapPairingDecorator::PairingSuccessfullyCompleted(const GapBondStrength& strength)
    {
        GapPairing::NotifyObservers([&strength](auto& obs)
            {
                obs.PairingSuccessfullyCompleted(strength);
            });
    }

    void GapPairingDecorator::PairingFailed(GapPairingResult error)
    {
        GapPairing::NotifyObservers([&error](auto& obs)
            {
                obs.PairingFailed(error);
            });
    }

    void GapPairingDecorator::OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData)
    {
        GapPairing::NotifyObservers([outOfBandData](auto& obs)
            {
                obs.OutOfBandDataGenerated(outOfBandData);
            });
    }

    GapRequestStatus GapPairingDecorator::PairAndBond(const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().PairAndBond(onDone);
    }

    GapRequestStatus GapPairingDecorator::AllowPairing(bool allow, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().AllowPairing(allow, onDone);
    }

    GapRequestStatus GapPairingDecorator::SetSecurityMode(SecurityModeAndLevel modeAndLevel, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().SetSecurityMode(modeAndLevel, onDone);
    }

    GapRequestStatus GapPairingDecorator::SetSecureConnectionsOnly(bool enabled, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().SetSecureConnectionsOnly(enabled, onDone);
    }

    GapRequestStatus GapPairingDecorator::SetIoCapabilities(IoCapabilities caps, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().SetIoCapabilities(caps, onDone);
    }

    GapRequestStatus GapPairingDecorator::GenerateOutOfBandData(const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().GenerateOutOfBandData(onDone);
    }

    GapRequestStatus GapPairingDecorator::SetOutOfBandData(const GapOutOfBandData& outOfBandData, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().SetOutOfBandData(outOfBandData, onDone);
    }

    GapRequestStatus GapPairingDecorator::AuthenticateWithPasskey(uint32_t passkey, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().AuthenticateWithPasskey(passkey, onDone);
    }

    GapRequestStatus GapPairingDecorator::NumericComparisonConfirm(bool accept, const infra::Function<void(GapPairingResult)>& onDone)
    {
        return GapPairingObserver::Subject().NumericComparisonConfirm(accept, onDone);
    }
}
