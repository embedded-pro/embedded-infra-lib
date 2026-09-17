#ifndef SERVICES_GAP_ADVERTISING_DATA_HPP
#define SERVICES_GAP_ADVERTISING_DATA_HPP

#include "infra/util/BoundedString.hpp"
#include "services/ble/Att.hpp"
#include "services/ble/GapTypes.hpp"
#include <optional>

namespace services
{
    class GapAdvertisingDataParser
    {
    public:
        // An AD structure starts wherever the structures before it end, so a 16-bit UUID list can
        // begin at an odd offset, and its values travel little-endian on air.
        static constexpr std::size_t maxListOf16BitUuids = (gapMaxAdvertisementDataSize - 2) / sizeof(AttAttribute::Uuid16);
        using ListOf16BitUuids = infra::BoundedVector<AttAttribute::Uuid16>::WithMaxSize<maxListOf16BitUuids>;

        // A 128-bit UUID travels little-endian on air, while AttAttribute::Uuid128 holds its
        // canonical order.
        static constexpr std::size_t maxListOf128BitUuids = (gapMaxAdvertisementDataSize - 2) / sizeof(AttAttribute::Uuid128);
        using ListOf128BitUuids = infra::BoundedVector<AttAttribute::Uuid128>::WithMaxSize<maxListOf128BitUuids>;

        explicit GapAdvertisingDataParser(infra::ConstByteRange data);

        infra::ConstByteRange LocalName() const;
        std::optional<std::pair<uint16_t, infra::ConstByteRange>> ManufacturerSpecificData() const;
        std::optional<GapAdvertisementFlags> Flags() const;
        void CompleteListOf16BitUuids(ListOf16BitUuids& result) const;
        void CompleteListOf128BitUuids(ListOf128BitUuids& result) const;

        // A peripheral with more UUIDs than fit advertises an incomplete list instead.
        void IncompleteListOf16BitUuids(ListOf16BitUuids& result) const;
        void IncompleteListOf128BitUuids(ListOf128BitUuids& result) const;

        std::optional<uint16_t> Appearance() const;
        std::optional<int8_t> TxPowerLevel() const;
        std::optional<std::pair<AttAttribute::Uuid16, infra::ConstByteRange>> ServiceData16BitUuid() const;
        std::optional<std::pair<AttAttribute::Uuid128, infra::ConstByteRange>> ServiceData128BitUuid() const;

    private:
        infra::ConstByteRange data;

    private:
        infra::ConstByteRange ParserAdvertisingData(GapAdvertisementDataType type) const;
        void ParseListOf16BitUuids(ListOf16BitUuids& result, GapAdvertisementDataType type) const;
        void ParseListOf128BitUuids(ListOf128BitUuids& result, GapAdvertisementDataType type) const;
    };

    class GapAdvertisementFormatter
    {
    public:
        explicit GapAdvertisementFormatter(infra::BoundedVector<uint8_t>& payload);

        void AppendFlags(GapAdvertisementFlags flags);
        void AppendCompleteLocalName(const infra::BoundedConstString& name);
        void AppendShortenedLocalName(const infra::BoundedConstString& name);
        void AppendManufacturerData(uint16_t manufacturerCode, infra::ConstByteRange data);
        void AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services);
        void AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services);

        void AppendIncompleteListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services);
        void AppendIncompleteListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services);

        void AppendPublicTargetAddress(hal::MacAddress address);
        void AppendAppearance(uint16_t appearance);

        // TX Power Level is a signed value in dBm, Assigned Numbers section 2.3.
        void AppendTxPowerLevel(int8_t txPowerLevel);
        void AppendServiceData(AttAttribute::Uuid16 uuid, infra::ConstByteRange data);
        void AppendServiceData(const AttAttribute::Uuid128& uuid, infra::ConstByteRange data);

        infra::ConstByteRange FormattedAdvertisementData() const;
        std::size_t RemainingSpaceAvailable() const;

    private:
        void AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services, GapAdvertisementDataType type);
        void AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services, GapAdvertisementDataType type);

        static constexpr std::size_t headerSize = 2;

        infra::BoundedVector<uint8_t>& payload;
    };
}

#endif
