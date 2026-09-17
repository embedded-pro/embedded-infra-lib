#ifndef SERVICES_GAP_TYPES_HPP
#define SERVICES_GAP_TYPES_HPP

#include "hal/interfaces/MacAddress.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/EnumCast.hpp"
#include <optional>

namespace services
{
    enum class GapDeviceAddressType : uint8_t
    {
        publicAddress,
        randomAddress,
    };

    enum class GapAdvertisementType : uint8_t
    {
        advInd,
        advNonconnInd
    };

    enum class GapAdvertisingEventType : uint8_t
    {
        advInd,
        advDirectInd,
        advScanInd,
        advNonconnInd,
        scanResponse,
    };

    // Values taken from Assigned Numbers, section 2.3 (Common Data Types)
    // Every value here is an assigned number; there is deliberately no sentinel among them.
    // ParserAdvertisingData already signals "not present" with an empty range.
    enum class GapAdvertisementDataType : uint8_t
    {
        flags = 0x01u,
        incompleteListOf16BitUuids = 0x02u,
        completeListOf16BitUuids = 0x03u,
        incompleteListOf128BitUuids = 0x06u,
        completeListOf128BitUuids = 0x07u,
        shortenedLocalName = 0x08u,
        completeLocalName = 0x09u,
        txPowerLevel = 0x0au,
        serviceData16BitUuid = 0x16u,
        publicTargetAddress = 0x17u,
        appearance = 0x19u,
        serviceData32BitUuid = 0x20u,
        serviceData128BitUuid = 0x21u,
        manufacturerSpecificData = 0xffu
    };

    // Values taken from Bluetooth Core Specification Supplement, Part A, section 1.3
    enum class GapAdvertisementFlags : uint8_t
    {
        leLimitedDiscoverableMode = 0x01u,
        leGeneralDiscoverableMode = 0x02u,
        brEdrNotSupported = 0x04u,
        leBrEdrController = 0x08u,
        leBrEdrHost = 0x10u
    };

    enum class GapRequestStatus : uint8_t
    {
        accepted = 0,
        invalidState,
        invalidParameter,
        busy,
        notSupported
    };

    // The legacy advertising PDU payload, Bluetooth Core Specification, Volume 6, Part B,
    // section 2.3.1.1. Extended advertising is not modelled; see docs/Ble.md.
    constexpr uint8_t gapMaxAdvertisementDataSize = 31;
    constexpr uint8_t gapMaxScanResponseDataSize = 31;

    struct GapConnectionParameters
    {
        using ConnectionIntervalMultiplier = uint16_t;                                           // Interval = Multiplier * 1.25 ms.
        static constexpr ConnectionIntervalMultiplier connectionIntervalMultiplierMin = 0x0006u; // 7.5 ms
        static constexpr ConnectionIntervalMultiplier connectionIntervalMultiplierMax = 0x0C80u; // 4000 ms

        using SupervisionTimeoutMultiplier = uint16_t;                                           // Timeout = Multiplier * 10 ms.
        static constexpr SupervisionTimeoutMultiplier supervisionTimeoutMultiplierMin = 0x000Au; // 100 ms
        static constexpr SupervisionTimeoutMultiplier supervisionTimeoutMultiplierMax = 0x0C80u; // 32000 ms

        ConnectionIntervalMultiplier minConnectionInterval;
        ConnectionIntervalMultiplier maxConnectionInterval;
        uint16_t peripheralLatency;
        SupervisionTimeoutMultiplier supervisionTimeout;
    };

    enum class GapPhy : uint8_t
    {
        le1M,
        le2M,
        leCoded
    };

    // Negotiated by the LE Set Data Length procedure, not by the connection parameter
    // update procedure.
    struct GapDataLength
    {
        static constexpr uint16_t initialMaxTxOctets = 251;

        // Time = (octets + 14) * 8 on LE 1M and LE 2M; LE Coded needs the S=8 coding,
        // hence the separate maximum.
        static constexpr uint16_t InitialMaxTxTime(GapPhy phy)
        {
            return phy == GapPhy::leCoded ? 17040u : static_cast<uint16_t>((initialMaxTxOctets + 14u) * 8u);
        }
    };

    // How much a bond is actually worth: whether it came from LE Secure Connections or legacy
    // pairing, whether its key is authenticated, and the negotiated key size. An application
    // deciding whether to trust a bonded peer with a privileged operation has no other basis for
    // the decision; Mode 1 Level 4 exists precisely to mean authenticated LESC with a 128-bit key.
    // Bluetooth Core Specification, Volume 3, Part H, section 2.4.5
    struct GapBondStrength
    {
        bool secureConnections;
        bool authenticated;
        uint8_t encryptionKeySize;

        bool operator==(const GapBondStrength& other) const = default;
    };

    struct GapAddress
    {
        hal::MacAddress address;
        GapDeviceAddressType type;

        bool operator==(GapAddress const& rhs) const
        {
            return type == rhs.type && address == rhs.address;
        }
    };

    struct GapOutOfBandData
    {
        hal::MacAddress macAddress;
        GapDeviceAddressType addressType;
        infra::ConstByteRange randomData;
        infra::ConstByteRange confirmData;
    };

    // Bluetooth Core Specification, Volume 4, Part E, section 7.7.65.2
    constexpr int8_t gapRssiNotAvailable = 127;

    struct GapAdvertisingReport
    {
        GapAdvertisingEventType eventType;
        GapDeviceAddressType addressType;
        hal::MacAddress address;

        // A view over the payload, valid for the duration of the DeviceDiscovered callback, in
        // the same way GattClientUpdateObserver::NotificationReceived hands over its data. An
        // observer that wants to keep it copies it into storage sized to its own needs.
        //
        // This is also what keeps the type from committing to legacy advertising: an owning
        // member bounded to 31 bytes could never carry an extended advertising payload, which
        // reaches 1650 bytes assembled across a chain. Extended advertising is not modelled here
        // (see docs/Ble.md), but the report no longer forecloses it.
        infra::ConstByteRange data;

        // Signed 8-bit as the specification defines it, where 127 means not available. An
        // int32_t left that convention nowhere to live.
        int8_t rssi;
    };

    inline GapAdvertisementFlags operator|(GapAdvertisementFlags lhs, GapAdvertisementFlags rhs)
    {
        return static_cast<GapAdvertisementFlags>(infra::enum_cast(lhs) | infra::enum_cast(rhs));
    }
}

namespace infra
{
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const services::GapAdvertisingEventType& eventType);
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const services::GapDeviceAddressType& addressType);
}

#endif
