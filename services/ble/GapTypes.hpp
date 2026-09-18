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

    // What a peripheral may be told to advertise. Directed advertising is not here because it is
    // meaningless without a peer address; see GapPeripheral::AdvertiseDirected.
    // Bluetooth Core Specification, Volume 6, Part B, section 2.3.1
    enum class GapAdvertisementType : uint8_t
    {
        advInd = 0,
        advNonconnInd = 1,
        advScanInd = 2
    };

    // High duty cycle directed advertising is the standard mechanism for fast reconnection to a
    // known peer. Its interval is fixed by the controller at no more than 3.75 ms and it must
    // stop within 1.28 s, so it takes no interval of its own.
    // Bluetooth Core Specification, Volume 6, Part B, section 4.4.2.4
    enum class GapDirectedAdvertisementType : uint8_t
    {
        highDutyCycle = 0,
        lowDutyCycle = 1
    };

    // The three primary advertising channels. At least one must be enabled.
    // Bluetooth Core Specification, Volume 6, Part B, section 4.4.2
    enum class GapAdvertisingChannels : uint8_t
    {
        channel37 = 0x01u,
        channel38 = 0x02u,
        channel39 = 0x04u,
        all = channel37 | channel38 | channel39
    };

    // Which scan and connection requests the controller acts on, the rest being those from
    // devices on the Filter Accept List. It is ignored for directed advertising, which names
    // the one peer it is aimed at.
    // Bluetooth Core Specification, Volume 4, Part E, section 7.8.5
    enum class GapAdvertisingFilterPolicy : uint8_t
    {
        any = 0x00u,
        filterScanRequests = 0x01u,
        filterConnectionRequests = 0x02u,
        filterScanAndConnectionRequests = 0x03u
    };

    // Bluetooth Core Specification, Volume 4, Part E, section 7.8.5
    struct GapAdvertisingParameters
    {
        using IntervalMultiplier = uint16_t;                                 // Interval = Multiplier * 0.625 ms.
        static constexpr IntervalMultiplier intervalMultiplierMin = 0x0020u; // 20 ms
        static constexpr IntervalMultiplier intervalMultiplierMax = 0x4000u; // 10240 ms

        GapAdvertisementType type;
        IntervalMultiplier interval;
        GapAdvertisingChannels channels;
        GapAdvertisingFilterPolicy filterPolicy;

        bool operator==(const GapAdvertisingParameters& other) const = default;
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
    // section 2.3.1.1.
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

        bool operator==(const GapConnectionParameters& other) const = default;

        // connSupervisionTimeout shall be larger than (1 + latency) * connInterval * 2, with the
        // timeout counted in units of 10 ms and the interval in units of 1.25 ms. Both sides are
        // multiplied by two to keep the comparison in whole numbers.
        // Bluetooth Core Specification, Volume 6, Part B, section 4.5.2
        constexpr bool SupervisionTimeoutIsLongEnough() const
        {
            return supervisionTimeout * 20u > (1u + peripheralLatency) * maxConnectionInterval * 5u;
        }
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

        // A packet carries the payload plus its overhead: access address 4, header 2, MIC 4 and
        // CRC 3, and a preamble of one octet on LE 1M and two on LE 2M. LE 1M sends an octet in
        // 8 us and LE 2M in half that, so the two PHYs do not share a time. LE Coded is fixed by
        // its S=8 coding rather than derived from the octet count.
        // Bluetooth Core Specification, Volume 6, Part B, sections 2.1 and 4.5.10
        static constexpr uint16_t InitialMaxTxTime(GapPhy phy)
        {
            switch (phy)
            {
                case GapPhy::le2M:
                    return static_cast<uint16_t>((initialMaxTxOctets + 15u) * 4u); // 1064 us
                case GapPhy::leCoded:
                    return 17040u;
                default:
                    return static_cast<uint16_t>((initialMaxTxOctets + 14u) * 8u); // 2120 us
            }
        }

        // The largest payload the link layer will carry, and the air time it may take.
        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.33
        static constexpr GapDataLength Maximum(GapPhy phy);

        uint16_t maxTxOctets;
        uint16_t maxTxTime;

        bool operator==(const GapDataLength& other) const = default;
    };

    constexpr GapDataLength GapDataLength::Maximum(GapPhy phy)
    {
        return GapDataLength{ initialMaxTxOctets, InitialMaxTxTime(phy) };
    }

    enum class GapScanType : uint8_t
    {
        passive = 0,
        active = 1
    };

    // Bluetooth Core Specification, Volume 4, Part E, section 7.8.10
    struct GapScanParameters
    {
        using IntervalMultiplier = uint16_t;                                 // Interval = Multiplier * 0.625 ms.
        static constexpr IntervalMultiplier intervalMultiplierMin = 0x0004u; //   2.5 ms
        static constexpr IntervalMultiplier intervalMultiplierMax = 0x4000u; // 10240 ms

        IntervalMultiplier interval;
        IntervalMultiplier window; // Must not exceed interval.
        GapScanType type;

        bool operator==(const GapScanParameters& other) const = default;
    };

    // Whether the bond came from LE Secure Connections or legacy pairing, whether its key is
    // authenticated, and the negotiated key size. Mode 1 Level 4 means authenticated LE Secure
    // Connections with a 128-bit key.
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

        infra::ConstByteRange data;

        // Signed 8-bit as the specification defines it, where 127 means not available.
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
