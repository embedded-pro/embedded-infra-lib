#include "services/ble/GapAdvertisingData.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/util/MemoryRange.hpp"

namespace
{
    void AddHeader(infra::BoundedVector<uint8_t>& payload, std::size_t length, services::GapAdvertisementDataType type)
    {
        payload.push_back(static_cast<uint8_t>(length + 1));
        payload.push_back(static_cast<uint8_t>(type));
    }

    void AddData(infra::BoundedVector<uint8_t>& payload, infra::ConstByteRange data)
    {
        payload.insert(payload.end(), data.begin(), data.end());
    }

    // Advertising data is little-endian on air, whatever the host is.
    void AddLittleEndian(infra::BoundedVector<uint8_t>& payload, uint16_t value)
    {
        payload.push_back(static_cast<uint8_t>(value));
        payload.push_back(static_cast<uint8_t>(value >> 8));
    }
}

namespace services
{
    GapAdvertisingDataParser::GapAdvertisingDataParser(infra::ConstByteRange data)
        : data(data)
    {}

    infra::ConstByteRange GapAdvertisingDataParser::LocalName() const
    {
        auto localName = ParserAdvertisingData(GapAdvertisementDataType::completeLocalName);

        if (localName.empty())
            return ParserAdvertisingData(GapAdvertisementDataType::shortenedLocalName);
        else
            return localName;
    }

    std::optional<std::pair<uint16_t, infra::ConstByteRange>> GapAdvertisingDataParser::ManufacturerSpecificData() const
    {
        infra::ByteInputStream stream(ParserAdvertisingData(GapAdvertisementDataType::manufacturerSpecificData), infra::softFail);
        auto manufacturerCode = infra::FromLittleEndian(stream.Extract<uint16_t>());
        auto manufacturerData = stream.Reader().Remaining();

        if (stream.Failed())
            return std::nullopt;

        return std::make_optional(std::make_pair(manufacturerCode, manufacturerData));
    }

    std::optional<GapAdvertisementFlags> GapAdvertisingDataParser::Flags() const
    {
        auto flagsData = ParserAdvertisingData(GapAdvertisementDataType::flags);

        if (flagsData.empty())
            return std::nullopt;

        if (flagsData.size() != 1)
            return std::nullopt;

        return std::make_optional(static_cast<GapAdvertisementFlags>(flagsData[0]));
    }

    void GapAdvertisingDataParser::CompleteListOf16BitUuids(ListOf16BitUuids& result) const
    {
        result.clear();

        auto uuidData = ParserAdvertisingData(GapAdvertisementDataType::completeListOf16BitUuids);

        if (uuidData.size() % sizeof(AttAttribute::Uuid16) != 0)
            return;

        infra::ByteInputStream stream(uuidData, infra::softFail);

        while (!stream.Empty() && !result.full())
            result.push_back(infra::FromLittleEndian(stream.Extract<AttAttribute::Uuid16>()));

        if (stream.Failed())
            result.clear();
    }

    infra::MemoryRange<const AttAttribute::Uuid128> GapAdvertisingDataParser::CompleteListOf128BitUuids() const
    {
        auto uuidData = ParserAdvertisingData(GapAdvertisementDataType::completeListOf128BitUuids);

        if (uuidData.size() % sizeof(AttAttribute::Uuid128) != 0)
            return {};

        // Unlike the 16-bit case this stays a view: Uuid128 is infra::BigEndian<std::array<uint8_t, 16>>,
        // whose alignment is 1, so no unaligned load is possible and no byte order is assumed here.
        return infra::ConstCastMemoryRange<AttAttribute::Uuid128>(infra::ReinterpretCastMemoryRange<const AttAttribute::Uuid128>(uuidData));
    }

    std::optional<uint16_t> GapAdvertisingDataParser::Appearance() const
    {
        auto appearanceData = ParserAdvertisingData(GapAdvertisementDataType::appearance);

        infra::ByteInputStream stream(appearanceData, infra::softFail);
        auto appearance = infra::FromLittleEndian(stream.Extract<uint16_t>());

        if (stream.Failed())
            return std::nullopt;

        return std::make_optional(appearance);
    }

    infra::ConstByteRange GapAdvertisingDataParser::ParserAdvertisingData(GapAdvertisementDataType type) const
    {
        const uint8_t lengthOffset = 0;
        const uint8_t advertisingTypeOffset = 1;
        const uint8_t headerSize = 2;

        infra::ConstByteRange advData = data;

        while (!advData.empty())
        {
            size_t elementSize = std::min<size_t>(advData[lengthOffset] + 1, advData.size());

            auto element = infra::Head(advData, elementSize);

            if (element.size() == 1 || (element.size() != element[lengthOffset] + 1))
                return infra::ConstByteRange();

            if (element[advertisingTypeOffset] == static_cast<uint8_t>(type))
                return infra::DiscardHead(element, headerSize);

            advData = infra::DiscardHead(advData, element.size());
        }

        return infra::ConstByteRange();
    }

    GapAdvertisementFormatter::GapAdvertisementFormatter(infra::BoundedVector<uint8_t>& payload)
        : payload(payload)
    {}

    void GapAdvertisementFormatter::AppendFlags(GapAdvertisementFlags flags)
    {
        really_assert(headerSize + sizeof(flags) <= RemainingSpaceAvailable());

        auto flagsByte = static_cast<uint8_t>(flags);
        AddHeader(payload, sizeof(flags), GapAdvertisementDataType::flags);
        AddData(payload, infra::MakeRangeFromSingleObject(flagsByte));
    }

    void GapAdvertisementFormatter::AppendCompleteLocalName(const infra::BoundedConstString& name)
    {
        really_assert(name.size() + headerSize <= RemainingSpaceAvailable() && name.size() > 0);

        AddHeader(payload, name.size(), GapAdvertisementDataType::completeLocalName);
        AddData(payload, infra::StringAsByteRange(name));
    }

    void GapAdvertisementFormatter::AppendShortenedLocalName(const infra::BoundedConstString& name)
    {
        really_assert(name.size() + headerSize <= RemainingSpaceAvailable() && name.size() > 0);

        AddHeader(payload, name.size(), GapAdvertisementDataType::shortenedLocalName);
        AddData(payload, infra::StringAsByteRange(name));
    }

    void GapAdvertisementFormatter::AppendManufacturerData(uint16_t manufacturerCode, infra::ConstByteRange data)
    {
        really_assert(data.size() + headerSize + sizeof(manufacturerCode) <= RemainingSpaceAvailable());

        AddHeader(payload, data.size() + sizeof(manufacturerCode), GapAdvertisementDataType::manufacturerSpecificData);
        AddLittleEndian(payload, manufacturerCode);
        AddData(payload, data);
    }

    void GapAdvertisementFormatter::AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services)
    {
        really_assert(services.size() * sizeof(AttAttribute::Uuid16) + headerSize <= RemainingSpaceAvailable() && !services.empty());

        AddHeader(payload, services.size() * sizeof(AttAttribute::Uuid16), GapAdvertisementDataType::completeListOf16BitUuids);

        for (const auto& service : services)
            AddLittleEndian(payload, service);
    }

    void GapAdvertisementFormatter::AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services)
    {
        really_assert(services.size() * sizeof(AttAttribute::Uuid128) + headerSize <= RemainingSpaceAvailable() && !services.empty());

        AddHeader(payload, services.size() * sizeof(AttAttribute::Uuid128), GapAdvertisementDataType::completeListOf128BitUuids);

        for (auto& service : services)
            AddData(payload, infra::ReinterpretCastMemoryRange<uint8_t>(infra::MakeRangeFromSingleObject(service)));
    }

    void GapAdvertisementFormatter::AppendPublicTargetAddress(hal::MacAddress address)
    {
        really_assert(sizeof(address) + headerSize <= RemainingSpaceAvailable());

        AddHeader(payload, sizeof(address), GapAdvertisementDataType::publicTargetAddress);
        AddData(payload, infra::ReinterpretCastMemoryRange<const uint8_t>(infra::MakeRangeFromSingleObject(address)));
    }

    void GapAdvertisementFormatter::AppendAppearance(uint16_t appearance)
    {
        really_assert(sizeof(appearance) + headerSize <= RemainingSpaceAvailable());

        AddHeader(payload, sizeof(appearance), GapAdvertisementDataType::appearance);
        AddLittleEndian(payload, appearance);
    }

    infra::ConstByteRange GapAdvertisementFormatter::FormattedAdvertisementData() const
    {
        return infra::MakeRange(payload);
    }

    std::size_t GapAdvertisementFormatter::RemainingSpaceAvailable() const
    {
        return payload.max_size() - payload.size();
    }
}
