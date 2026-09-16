#ifndef SERVICES_GAP_TYPES_HPP
#define SERVICES_GAP_TYPES_HPP

#include "hal/interfaces/MacAddress.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/EnumCast.hpp"

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

    enum class GapState : uint8_t
    {
        standby,
        scanning,
        advertising,
        connected,
        initiating
    };

    enum class GapAdvertisingEventType : uint8_t
    {
        advInd,
        advDirectInd,
        advScanInd,
        advNonconnInd,
        scanResponse,
    };

    enum class GapAdvertisementDataType : uint8_t
    {
        unknownType = 0x00u,
        flags = 0x01u,
        completeListOf16BitUuids = 0x03u,
        completeListOf128BitUuids = 0x07u,
        shortenedLocalName = 0x08u,
        completeLocalName = 0x09u,
        publicTargetAddress = 0x17u,
        appearance = 0x19u,
        manufacturerSpecificData = 0xffu
    };

    enum class GapAdvertisementFlags : uint8_t
    {
        leLimitedDiscoverableMode = 0x01u,
        leGeneralDiscoverableMode = 0x02u,
        brEdrNotSupported = 0x04u,
        leBrEdrController = 0x08u,
        leBrEdrHost = 0x10u
    };

    // Status of a request at the moment it is issued; a request that is not
    // accepted never results in a call to its completion callback.
    enum class GapRequestStatus : uint8_t
    {
        accepted = 0,
        invalidState,
        invalidParameter,
        busy,
        notSupported
    };

    constexpr uint8_t gapMaxAdvertisementDataSize = 31;
    constexpr uint8_t gapMaxScanResponseDataSize = 31;

    struct GapConnectionParameters
    {
        uint16_t minConnIntMultiplier;
        uint16_t maxConnIntMultiplier;
        uint16_t slaveLatency;
        uint16_t supervisorTimeoutMs;

        static constexpr uint16_t connectionInitialMaxTxOctets = 251;
        static constexpr uint16_t connectionInitialMaxTxTime = 2120; // (connectionInitialMaxTxOctets + 14) * 8
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

    struct GapAdvertisingReport
    {
        GapAdvertisingEventType eventType;
        GapDeviceAddressType addressType;
        hal::MacAddress address;
        infra::BoundedVector<uint8_t>::WithMaxSize<gapMaxAdvertisementDataSize> data;
        int32_t rssi;
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
    infra::TextOutputStream& operator<<(infra::TextOutputStream& stream, const services::GapState& state);
}

#endif
