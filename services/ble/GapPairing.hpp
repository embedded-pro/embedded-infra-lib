#ifndef SERVICES_GAP_PAIRING_HPP
#define SERVICES_GAP_PAIRING_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    enum class GapPairingResult : uint8_t
    {
        success = 0,
        passkeyEntryFailed,
        authenticationRequirementsNotMet,
        pairingNotSupported,
        insufficientEncryptionKeySize,
        numericComparisonFailed,
        timeout,
        encryptionFailed,
        unknown,
    };

    class GapPairing;

    class GapPairingObserver
        : public infra::Observer<GapPairingObserver, GapPairing>
    {
    public:
        using infra::Observer<GapPairingObserver, GapPairing>::Observer;

        virtual void DisplayPasskey(int32_t passkey, bool numericComparison) = 0;
        virtual void PairingSuccessfullyCompleted() = 0;
        virtual void PairingFailed(GapPairingResult error) = 0;
        virtual void OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData) = 0;
    };

    class GapPairing
        : public infra::Subject<GapPairingObserver>
    {
    public:
        enum class IoCapabilities : uint8_t
        {
            display,
            displayYesNo,
            keyboard,
            none,
            keyboardDisplay
        };

        enum class SecurityMode : uint8_t
        {
            mode1,
            mode2
        };

        enum class SecurityLevel : uint8_t
        {
            level1,
            level2,
            level3,
            level4,
        };

        // Each procedure below reports whether the request is accepted through its
        // return value, and its outcome through onDone. onDone is never invoked from
        // within the call itself; it is scheduled on the event dispatcher. A request
        // that is not accepted never results in a call to onDone. Pairing that is
        // initiated by the peer instead of by PairAndBond is reported through the
        // observer.

        // 1. If there is a pre-existing bond, then the connection will be encrypted.
        // 2. If there is no pre-existing bond, then pairing, encrypting, and bonding (storing the keys) will take place.
        virtual GapRequestStatus PairAndBond(const infra::Function<void(GapPairingResult)>& onDone) = 0;

        virtual GapRequestStatus AllowPairing(bool allow, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus SetSecurityMode(SecurityMode mode, SecurityLevel level, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus SetIoCapabilities(IoCapabilities caps, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus GenerateOutOfBandData(const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus SetOutOfBandData(const GapOutOfBandData& outOfBandData, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus AuthenticateWithPasskey(uint32_t passkey, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus NumericComparisonConfirm(bool accept, const infra::Function<void(GapPairingResult)>& onDone) = 0;
    };

    class GapPairingDecorator
        : public GapPairingObserver
        , public GapPairing
    {
    public:
        using GapPairingObserver::GapPairingObserver;

        // Implementation of GapPairingObserver
        void DisplayPasskey(int32_t passkey, bool numericComparison) override;
        void PairingSuccessfullyCompleted() override;
        void PairingFailed(GapPairingResult error) override;
        void OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData) override;

        // Implementation of GapPairing
        GapRequestStatus PairAndBond(const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus AllowPairing(bool allow, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetSecurityMode(SecurityMode mode, SecurityLevel level, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetIoCapabilities(IoCapabilities caps, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus GenerateOutOfBandData(const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetOutOfBandData(const GapOutOfBandData& outOfBandData, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus AuthenticateWithPasskey(uint32_t passkey, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus NumericComparisonConfirm(bool accept, const infra::Function<void(GapPairingResult)>& onDone) override;
    };
}

#endif
