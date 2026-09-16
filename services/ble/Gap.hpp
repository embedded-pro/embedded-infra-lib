#ifndef SERVICES_GAP_HPP
#define SERVICES_GAP_HPP

#include "infra/timer/Timer.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GapAdvertisingData.hpp"
#include "services/ble/GapTypes.hpp"
#include <optional>

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

    class GapBonding;

    class GapBondingObserver
        : public infra::Observer<GapBondingObserver, GapBonding>
    {
    public:
        using infra::Observer<GapBondingObserver, GapBonding>::Observer;

        virtual void NumberOfBondsChanged(std::size_t nrBonds) = 0;
    };

    class GapBonding
        : public infra::Subject<GapBondingObserver>
    {
    public:
        virtual void RemoveAllBonds() = 0;
        virtual void RemoveOldestBond() = 0;

        virtual std::size_t GetMaxNumberOfBonds() const = 0;
        virtual std::size_t GetNumberOfBonds() const = 0;
        virtual bool IsDeviceBonded(hal::MacAddress address, GapDeviceAddressType addressType) const = 0;
    };

    class GapBondingDecorator
        : public GapBondingObserver
        , public GapBonding
    {
    public:
        using GapBondingObserver::GapBondingObserver;

        // Implementation of GapBondingObserver
        void NumberOfBondsChanged(std::size_t nrBonds) override;

        // Implementation of GapBonding
        void RemoveAllBonds() override;
        void RemoveOldestBond() override;
        std::size_t GetMaxNumberOfBonds() const override;
        std::size_t GetNumberOfBonds() const override;
        bool IsDeviceBonded(hal::MacAddress address, GapDeviceAddressType addressType) const override;
    };

    class GapPeripheral;

    class GapPeripheralObserver
        : public infra::Observer<GapPeripheralObserver, GapPeripheral>
    {
    public:
        using infra::Observer<GapPeripheralObserver, GapPeripheral>::Observer;

        virtual void StateChanged(GapState state) = 0;
    };

    class GapPeripheral
        : public infra::Subject<GapPeripheralObserver>
    {
    public:
        using AdvertisementIntervalMultiplier = uint16_t;                                              // Interval = Multiplier * 0.625 ms.
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMin = 0x20u;   // 20 ms
        static constexpr AdvertisementIntervalMultiplier advertisementIntervalMultiplierMax = 0x4000u; // 10240 ms

    public:
        virtual GapAddress GetAddress() const = 0;
        virtual GapAddress GetIdentityAddress() const = 0;
        virtual void SetAdvertisementData(infra::ConstByteRange data) = 0;
        virtual infra::ConstByteRange GetAdvertisementData() const = 0;
        virtual void SetScanResponseData(infra::ConstByteRange data) = 0;
        virtual infra::ConstByteRange GetScanResponseData() const = 0;
        virtual void Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier) = 0;
        virtual void Standby() = 0;
        virtual void SetConnectionParameters(const services::GapConnectionParameters& connParam) = 0;
    };

    class GapPeripheralDecorator
        : public GapPeripheralObserver
        , public GapPeripheral
    {
    public:
        using GapPeripheralObserver::GapPeripheralObserver;

        // Implementation of GapPeripheralObserver
        void StateChanged(GapState state) override;

        // Implementation of GapPeripheral
        GapAddress GetAddress() const override;
        GapAddress GetIdentityAddress() const override;

        void SetAdvertisementData(infra::ConstByteRange data) override;
        infra::ConstByteRange GetAdvertisementData() const override;
        void SetScanResponseData(infra::ConstByteRange data) override;
        infra::ConstByteRange GetScanResponseData() const override;
        void Advertise(GapAdvertisementType type, AdvertisementIntervalMultiplier multiplier) override;
        void Standby() override;
        void SetConnectionParameters(const services::GapConnectionParameters& connParam) override;
    };

    class GapCentral;

    class GapCentralObserver
        : public infra::Observer<GapCentralObserver, GapCentral>
    {
    public:
        using infra::Observer<GapCentralObserver, GapCentral>::Observer;

        virtual void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) = 0;
        virtual void StateChanged(GapState state) = 0;
    };

    class GapCentral
        : public infra::Subject<GapCentralObserver>
    {
    public:
        virtual void Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout) = 0;
        virtual void CancelConnect() = 0;
        virtual void Disconnect() = 0;
        virtual void SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType) = 0;
        virtual void StartDeviceDiscovery() = 0;
        virtual void StopDeviceDiscovery() = 0;
        virtual std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const = 0;
    };

    class GapCentralDecorator
        : public GapCentralObserver
        , public GapCentral
    {
    public:
        using GapCentralObserver::GapCentralObserver;

        // Implementation of GapCentralObserver
        void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) override;
        void StateChanged(GapState state) override;

        // Implementation of GapCentral
        void Connect(hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout) override;
        void CancelConnect() override;
        void Disconnect() override;
        void SetAddress(hal::MacAddress macAddress, GapDeviceAddressType addressType) override;
        void StartDeviceDiscovery() override;
        void StopDeviceDiscovery() override;
        std::optional<hal::MacAddress> ResolvePrivateAddress(hal::MacAddress address) const override;
    };
}

#endif
