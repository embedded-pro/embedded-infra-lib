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

    void GapPairingDecorator::PairingSuccessfullyCompleted()
    {
        GapPairing::NotifyObservers([](auto& obs)
            {
                obs.PairingSuccessfullyCompleted();
            });
    }

    void GapPairingDecorator::PairingFailed(PairingErrorType error)
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

    void GapPairingDecorator::PairAndBond()
    {
        GapPairingObserver::Subject().PairAndBond();
    }

    void GapPairingDecorator::AllowPairing(bool allow)
    {
        GapPairingObserver::Subject().AllowPairing(allow);
    }

    void GapPairingDecorator::SetSecurityMode(SecurityMode mode, SecurityLevel level)
    {
        GapPairingObserver::Subject().SetSecurityMode(mode, level);
    }

    void GapPairingDecorator::SetIoCapabilities(IoCapabilities caps)
    {
        GapPairingObserver::Subject().SetIoCapabilities(caps);
    }

    void GapPairingDecorator::GenerateOutOfBandData()
    {
        GapPairingObserver::Subject().GenerateOutOfBandData();
    }

    void GapPairingDecorator::SetOutOfBandData(const GapOutOfBandData& outOfBandData)
    {
        GapPairingObserver::Subject().SetOutOfBandData(outOfBandData);
    }

    void GapPairingDecorator::AuthenticateWithPasskey(uint32_t passkey)
    {
        GapPairingObserver::Subject().AuthenticateWithPasskey(passkey);
    }

    void GapPairingDecorator::NumericComparisonConfirm(bool accept)
    {
        GapPairingObserver::Subject().NumericComparisonConfirm(accept);
    }
}
