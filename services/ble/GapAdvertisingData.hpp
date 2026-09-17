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
        // An AD structure starts wherever the structures before it end, so a 16-bit UUID list
        // can begin at an odd offset. Decoding it in place would need an unaligned 16-bit load,
        // which faults on ARMv6-M, and would read the on-air little-endian bytes in host order.
        // The list is therefore decoded into the caller's storage rather than viewed in place.
        static constexpr std::size_t maxListOf16BitUuids = (gapMaxAdvertisementDataSize - 2) / sizeof(AttAttribute::Uuid16);
        using ListOf16BitUuids = infra::BoundedVector<AttAttribute::Uuid16>::WithMaxSize<maxListOf16BitUuids>;

        explicit GapAdvertisingDataParser(infra::ConstByteRange data);

        infra::ConstByteRange LocalName() const;
        std::optional<std::pair<uint16_t, infra::ConstByteRange>> ManufacturerSpecificData() const;
        std::optional<GapAdvertisementFlags> Flags() const;
        void CompleteListOf16BitUuids(ListOf16BitUuids& result) const;
        infra::MemoryRange<const AttAttribute::Uuid128> CompleteListOf128BitUuids() const;
        std::optional<uint16_t> Appearance() const;

    private:
        infra::ConstByteRange data;

    private:
        infra::ConstByteRange ParserAdvertisingData(GapAdvertisementDataType type) const;
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
        void AppendPublicTargetAddress(hal::MacAddress address);
        void AppendAppearance(uint16_t appearance);

        infra::ConstByteRange FormattedAdvertisementData() const;
        std::size_t RemainingSpaceAvailable() const;

    private:
        static constexpr std::size_t headerSize = 2;

        infra::BoundedVector<uint8_t>& payload;
    };
}

#endif
