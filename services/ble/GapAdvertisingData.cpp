#include "services/ble/GapAdvertisingData.hpp"
#include "infra/stream/ByteInputStream.hpp"
#include "infra/util/MemoryRange.hpp"
#include <algorithm>
#include <array>

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

    // A 128-bit UUID travels least significant octet first, so its on-air order is the reverse of
    // the canonical order AttAttribute::Uuid128 converts to and from.
    void AddLittleEndian(infra::BoundedVector<uint8_t>& payload, const services::AttAttribute::Uuid128& uuid)
    {
        const std::array<uint8_t, 16> canonical = uuid;

        payload.insert(payload.end(), canonical.rbegin(), canonical.rend());
    }

    services::AttAttribute::Uuid128 Uuid128FromLittleEndian(infra::ConstByteRange onAir)
    {
        std::array<uint8_t, 16> canonical{};
        std::copy(onAir.begin(), onAir.end(), canonical.rbegin());

        return services::AttAttribute::Uuid128{ canonical };
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
        ParseListOf16BitUuids(result, GapAdvertisementDataType::completeListOf16BitUuids);
    }

    void GapAdvertisingDataParser::IncompleteListOf16BitUuids(ListOf16BitUuids& result) const
    {
        ParseListOf16BitUuids(result, GapAdvertisementDataType::incompleteListOf16BitUuids);
    }

    void GapAdvertisingDataParser::CompleteListOf128BitUuids(ListOf128BitUuids& result) const
    {
        ParseListOf128BitUuids(result, GapAdvertisementDataType::completeListOf128BitUuids);
    }

    void GapAdvertisingDataParser::IncompleteListOf128BitUuids(ListOf128BitUuids& result) const
    {
        ParseListOf128BitUuids(result, GapAdvertisementDataType::incompleteListOf128BitUuids);
    }

    void GapAdvertisingDataParser::ParseListOf16BitUuids(ListOf16BitUuids& result, GapAdvertisementDataType type) const
    {
        result.clear();

        auto uuidData = ParserAdvertisingData(type);

        if (uuidData.size() % sizeof(AttAttribute::Uuid16) != 0)
            return;

        infra::ByteInputStream stream(uuidData, infra::softFail);

        while (!stream.Empty() && !result.full())
            result.push_back(infra::FromLittleEndian(stream.Extract<AttAttribute::Uuid16>()));

        if (stream.Failed())
            result.clear();
    }

    void GapAdvertisingDataParser::ParseListOf128BitUuids(ListOf128BitUuids& result, GapAdvertisementDataType type) const
    {
        result.clear();

        auto uuidData = ParserAdvertisingData(type);

        if (uuidData.size() % sizeof(AttAttribute::Uuid128) != 0)
            return;

        while (!uuidData.empty() && !result.full())
        {
            result.push_back(Uuid128FromLittleEndian(infra::Head(uuidData, sizeof(AttAttribute::Uuid128))));
            uuidData = infra::DiscardHead(uuidData, sizeof(AttAttribute::Uuid128));
        }
    }

    std::optional<int8_t> GapAdvertisingDataParser::TxPowerLevel() const
    {
        auto txPowerData = ParserAdvertisingData(GapAdvertisementDataType::txPowerLevel);

        if (txPowerData.size() != 1)
            return std::nullopt;

        return std::make_optional(static_cast<int8_t>(txPowerData[0]));
    }

    std::optional<std::pair<AttAttribute::Uuid16, infra::ConstByteRange>> GapAdvertisingDataParser::ServiceData16BitUuid() const
    {
        infra::ByteInputStream stream(ParserAdvertisingData(GapAdvertisementDataType::serviceData16BitUuid), infra::softFail);
        auto uuid = infra::FromLittleEndian(stream.Extract<AttAttribute::Uuid16>());
        auto serviceData = stream.Reader().Remaining();

        if (stream.Failed())
            return std::nullopt;

        return std::make_optional(std::make_pair(uuid, serviceData));
    }

    std::optional<std::pair<AttAttribute::Uuid128, infra::ConstByteRange>> GapAdvertisingDataParser::ServiceData128BitUuid() const
    {
        auto serviceData = ParserAdvertisingData(GapAdvertisementDataType::serviceData128BitUuid);

        if (serviceData.size() < sizeof(AttAttribute::Uuid128))
            return std::nullopt;

        auto uuid = Uuid128FromLittleEndian(infra::Head(serviceData, sizeof(AttAttribute::Uuid128)));

        return std::make_optional(std::make_pair(uuid, infra::DiscardHead(serviceData, sizeof(AttAttribute::Uuid128))));
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
        AppendListOfServicesUuid(services, GapAdvertisementDataType::completeListOf16BitUuids);
    }

    void GapAdvertisementFormatter::AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services)
    {
        AppendListOfServicesUuid(services, GapAdvertisementDataType::completeListOf128BitUuids);
    }

    void GapAdvertisementFormatter::AppendIncompleteListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services)
    {
        AppendListOfServicesUuid(services, GapAdvertisementDataType::incompleteListOf16BitUuids);
    }

    void GapAdvertisementFormatter::AppendIncompleteListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services)
    {
        AppendListOfServicesUuid(services, GapAdvertisementDataType::incompleteListOf128BitUuids);
    }

    void GapAdvertisementFormatter::AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid16> services, GapAdvertisementDataType type)
    {
        really_assert(services.size() * sizeof(AttAttribute::Uuid16) + headerSize <= RemainingSpaceAvailable() && !services.empty());

        AddHeader(payload, services.size() * sizeof(AttAttribute::Uuid16), type);

        for (const auto& service : services)
            AddLittleEndian(payload, service);
    }

    void GapAdvertisementFormatter::AppendListOfServicesUuid(infra::MemoryRange<AttAttribute::Uuid128> services, GapAdvertisementDataType type)
    {
        really_assert(services.size() * sizeof(AttAttribute::Uuid128) + headerSize <= RemainingSpaceAvailable() && !services.empty());

        AddHeader(payload, services.size() * sizeof(AttAttribute::Uuid128), type);

        for (const auto& service : services)
            AddLittleEndian(payload, service);
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

    void GapAdvertisementFormatter::AppendTxPowerLevel(int8_t txPowerLevel)
    {
        really_assert(sizeof(txPowerLevel) + headerSize <= RemainingSpaceAvailable());

        AddHeader(payload, sizeof(txPowerLevel), GapAdvertisementDataType::txPowerLevel);
        payload.push_back(static_cast<uint8_t>(txPowerLevel));
    }

    void GapAdvertisementFormatter::AppendServiceData(AttAttribute::Uuid16 uuid, infra::ConstByteRange data)
    {
        really_assert(data.size() + headerSize + sizeof(uuid) <= RemainingSpaceAvailable());

        AddHeader(payload, data.size() + sizeof(uuid), GapAdvertisementDataType::serviceData16BitUuid);
        AddLittleEndian(payload, uuid);
        AddData(payload, data);
    }

    void GapAdvertisementFormatter::AppendServiceData(const AttAttribute::Uuid128& uuid, infra::ConstByteRange data)
    {
        really_assert(data.size() + headerSize + sizeof(uuid) <= RemainingSpaceAvailable());

        AddHeader(payload, data.size() + sizeof(uuid), GapAdvertisementDataType::serviceData128BitUuid);
        AddLittleEndian(payload, uuid);
        AddData(payload, data);
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
