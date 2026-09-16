#ifndef SERVICES_GAP_PAIRING_HPP
#define SERVICES_GAP_PAIRING_HPP

#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    class GapPairing;

    class GapPairingObserver
        : public infra::Observer<GapPairingObserver, GapPairing>
    {
    public:
        using infra::Observer<GapPairingObserver, GapPairing>::Observer;

        enum class PairingErrorType : uint8_t
        {
            passkeyEntryFailed,
            authenticationRequirementsNotMet,
            pairingNotSupported,
            insufficientEncryptionKeySize,
            numericComparisonFailed,
            timeout,
            encryptionFailed,
            unknown,
        };

        virtual void DisplayPasskey(int32_t passkey, bool numericComparison) = 0;
        virtual void PairingSuccessfullyCompleted() = 0;
        virtual void PairingFailed(PairingErrorType error) = 0;
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

        // 1. If there is a pre-existing bond, then the connection will be encrypted.
        // 2. If there is no pre-existing bond, then pairing, encrypting, and bonding (storing the keys) will take place.
        virtual void PairAndBond() = 0;

        virtual void AllowPairing(bool allow) = 0;
        virtual void SetSecurityMode(SecurityMode mode, SecurityLevel level) = 0;
        virtual void SetIoCapabilities(IoCapabilities caps) = 0;
        virtual void GenerateOutOfBandData() = 0;
        virtual void SetOutOfBandData(const GapOutOfBandData& outOfBandData) = 0;
        virtual void AuthenticateWithPasskey(uint32_t passkey) = 0;
        virtual void NumericComparisonConfirm(bool accept) = 0;
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
        void PairingFailed(PairingErrorType error) override;
        void OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData) override;

        // Implementation of GapPairing
        void PairAndBond() override;
        void AllowPairing(bool allow) override;
        void SetSecurityMode(SecurityMode mode, SecurityLevel level) override;
        void SetIoCapabilities(IoCapabilities caps) override;
        void GenerateOutOfBandData() override;
        void SetOutOfBandData(const GapOutOfBandData& outOfBandData) override;
        void AuthenticateWithPasskey(uint32_t passkey) override;
        void NumericComparisonConfirm(bool accept) override;
    };
}

#endif
