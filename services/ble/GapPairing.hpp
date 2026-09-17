#ifndef SERVICES_GAP_PAIRING_HPP
#define SERVICES_GAP_PAIRING_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapTypes.hpp"

namespace services
{
    // The values below unknown are appended rather than inserted: TestGapProto asserts these
    // against their proto counterparts value by value, so renumbering any of them would break
    // every downstream port silently.
    //
    // Several of the appended codes separate a benign failure from an active attack, which is the
    // distinction a security-conscious application would log or act on. Unspecified Reason, 0x08,
    // is what unknown already means and gains nothing from a name of its own.
    // Bluetooth Core Specification, Volume 3, Part H, section 3.5.5
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
        oobNotAvailable,
        confirmValueFailed,
        commandNotSupported,
        repeatedAttempts,
        invalidParameters,
        dhKeyCheckFailed,
        brEdrPairingInProgress,
        crossTransportKeyDerivationNotAllowed,
        keyRejected
    };

    class GapPairing;

    class GapPairingObserver
        : public infra::Observer<GapPairingObserver, GapPairing>
    {
    public:
        using infra::Observer<GapPairingObserver, GapPairing>::Observer;

        // Two distinct Security Manager procedures, so two callbacks rather than one
        // discriminated by a bool at the call site. Passkey Entry asks the user to read a passkey
        // off this device and type it into the peer; Numeric Comparison asks them to confirm that
        // both devices show the same value. The value is six digits, 000000 to 999999.
        // Bluetooth Core Specification, Volume 3, Part H, section 2.3.5.6
        virtual void DisplayPasskey(uint32_t passkey) = 0;
        virtual void ConfirmNumericComparison(uint32_t value) = 0;
        virtual void PairingSuccessfullyCompleted(const GapBondStrength& strength) = 0;
        virtual void PairingFailed(GapPairingResult error) = 0;
        virtual void OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData) = 0;
    };

    class GapPairing
        : public infra::Subject<GapPairingObserver>
    {
    public:
        // Values taken from Bluetooth Core Specification
        // Volume 3, Part H, section 3.3.1, Table 3.4 (IO Capability)
        enum class IoCapabilities : uint8_t
        {
            display = 0x00u,
            displayYesNo = 0x01u,
            keyboard = 0x02u,
            none = 0x03u,
            keyboardDisplay = 0x04u
        };

        // The six combinations the specification defines, and only those. Mode 2 has levels 1
        // and 2 only, so a separate mode and level would make SetSecurityMode(mode2, level4)
        // expressible and meaningless.
        // Bluetooth Core Specification, Volume 3, Part C, sections 10.2.1 and 10.2.2
        enum class SecurityModeAndLevel : uint8_t
        {
            mode1Level1 = 0, // No security
            mode1Level2,     // Unauthenticated pairing with encryption
            mode1Level3,     // Authenticated pairing with encryption
            mode1Level4,     // Authenticated LE Secure Connections with a 128-bit key
            mode2Level1,     // Unauthenticated pairing with data signing
            mode2Level2      // Authenticated pairing with data signing
        };

        // 1. If there is a pre-existing bond, then the connection will be encrypted.
        // 2. If there is no pre-existing bond, then pairing, encrypting, and bonding (storing the keys) will take place.
        virtual GapRequestStatus PairAndBond(const infra::Function<void(GapPairingResult)>& onDone) = 0;

        virtual GapRequestStatus AllowPairing(bool allow, const infra::Function<void(GapPairingResult)>& onDone) = 0;
        virtual GapRequestStatus SetSecurityMode(SecurityModeAndLevel modeAndLevel, const infra::Function<void(GapPairingResult)>& onDone) = 0;

        // Secure Connections Only is a property of the device, not of one link: while it is on,
        // every service requiring security requires Mode 1 Level 4. It is therefore its own
        // procedure rather than a seventh value above, which would conflate the two.
        // Bluetooth Core Specification, Volume 3, Part C, section 10.2.4
        virtual GapRequestStatus SetSecureConnectionsOnly(bool enabled, const infra::Function<void(GapPairingResult)>& onDone) = 0;
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
        void DisplayPasskey(uint32_t passkey) override;
        void ConfirmNumericComparison(uint32_t value) override;
        void PairingSuccessfullyCompleted(const GapBondStrength& strength) override;
        void PairingFailed(GapPairingResult error) override;
        void OutOfBandDataGenerated(const GapOutOfBandData& outOfBandData) override;

        // Implementation of GapPairing
        GapRequestStatus PairAndBond(const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus AllowPairing(bool allow, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetSecurityMode(SecurityModeAndLevel modeAndLevel, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetSecureConnectionsOnly(bool enabled, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetIoCapabilities(IoCapabilities caps, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus GenerateOutOfBandData(const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus SetOutOfBandData(const GapOutOfBandData& outOfBandData, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus AuthenticateWithPasskey(uint32_t passkey, const infra::Function<void(GapPairingResult)>& onDone) override;
        GapRequestStatus NumericComparisonConfirm(bool accept, const infra::Function<void(GapPairingResult)>& onDone) override;
    };
}

#endif
