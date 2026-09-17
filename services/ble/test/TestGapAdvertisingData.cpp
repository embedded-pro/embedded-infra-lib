#include "infra/util/ByteRange.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "services/ble/GapAdvertisingData.hpp"
#include "gmock/gmock.h"

namespace services
{
    TEST(GapAdvertisingDataParserTest, payload_too_small)
    {
        std::array<uint8_t, 1> data{ { 0x00 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_EQ(infra::ConstByteRange(), gapAdvertisingDataParser.LocalName());
        EXPECT_FALSE(gapAdvertisingDataParser.ManufacturerSpecificData());
    }

    TEST(GapAdvertisingDataParserTest, payload_does_not_contain_valid_info)
    {
        std::array<uint8_t, 2> data{ { 0x03, 0x02 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_EQ(infra::ConstByteRange(), gapAdvertisingDataParser.LocalName());
        EXPECT_FALSE(gapAdvertisingDataParser.ManufacturerSpecificData());
    }

    TEST(GapAdvertisingDataParserTest, payload_does_not_contain_valid_length)
    {
        const std::array<uint8_t, 14> data{ { 0x05, 0xff, 0xaa, 0xbb, 0xcc, 0xdd, 0xaa, 0x09, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67 } };
        const std::array<uint8_t, 2> payloadParser{ { 0xcc, 0xdd } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        auto manufacturerSpecificData = gapAdvertisingDataParser.ManufacturerSpecificData();

        EXPECT_EQ(infra::ConstByteRange(), gapAdvertisingDataParser.LocalName());
        EXPECT_TRUE(manufacturerSpecificData);
        EXPECT_EQ(0xbbaa, manufacturerSpecificData->first);
        EXPECT_TRUE(infra::ContentsEqual(infra::MakeRange(payloadParser), manufacturerSpecificData->second));
    }

    TEST(GapAdvertisingDataParserTest, get_local_name_using_type_shortenedLocalName)
    {
        std::array<uint8_t, 5> data{ { 0x04, 0x08, 0x73, 0x74, 0x72 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_EQ("str", ByteRangeAsStdString(gapAdvertisingDataParser.LocalName()));
        EXPECT_FALSE(gapAdvertisingDataParser.ManufacturerSpecificData());
    }

    TEST(GapAdvertisingDataParserTest, get_local_name_using_type_completeLocalName)
    {
        std::array<uint8_t, 8> data{ { 0x07, 0x09, 0x73, 0x74, 0x72, 0x69, 0x6E, 0x67 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_EQ("string", ByteRangeAsStdString(gapAdvertisingDataParser.LocalName()));
        EXPECT_FALSE(gapAdvertisingDataParser.ManufacturerSpecificData());
    }

    TEST(GapAdvertisingDataParserTest, get_manufacturer_specific_data)
    {
        const std::array<uint8_t, 6> data{ { 0x05, 0xff, 0xaa, 0xbb, 0xcc, 0xdd } };
        const std::array<uint8_t, 2> payloadParser{ { 0xcc, 0xdd } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        auto manufacturerSpecificData = gapAdvertisingDataParser.ManufacturerSpecificData();

        EXPECT_EQ(infra::ConstByteRange(), gapAdvertisingDataParser.LocalName());
        EXPECT_TRUE(manufacturerSpecificData);
        EXPECT_EQ(0xbbaa, manufacturerSpecificData->first);
        EXPECT_TRUE(infra::ContentsEqual(infra::MakeRange(payloadParser), manufacturerSpecificData->second));
    }

    TEST(GapAdvertisingDataParserTest, get_appearance_value)
    {
        const std::array<uint8_t, 4> data = { 0x03, 0x19, 0xC1, 0x03 };

        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        auto appearance = parser.Appearance();

        ASSERT_TRUE(appearance);
        EXPECT_EQ(0x03C1, *appearance);
    }

    TEST(GapAdvertisingDataParserTest, useful_info_after_first_ad_structure)
    {
        const std::array<uint8_t, 21> data{ { 0x02, 0x01, 0x06, 0x08, 0x09, 0x70, 0x68, 0x69, 0x6C, 0x69, 0x70, 0x73, 0x02, 0x0a, 0x08, 0x05, 0xff, 0xaa, 0xbb, 0xcc, 0xdd } };
        const std::array<uint8_t, 2> payloadParser{ { 0xcc, 0xdd } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        auto manufacturerSpecificData = gapAdvertisingDataParser.ManufacturerSpecificData();

        EXPECT_EQ("philips", ByteRangeAsStdString(gapAdvertisingDataParser.LocalName()));
        EXPECT_TRUE(manufacturerSpecificData);
        EXPECT_EQ(0xbbaa, manufacturerSpecificData->first);
        EXPECT_TRUE(infra::ContentsEqual(infra::MakeRange(payloadParser), manufacturerSpecificData->second));
    }

    TEST(GapAdvertisingDataParserTest, complete_list_of_16bit_services)
    {
        const std::array<uint8_t, 6> data{ { 0x05, 0x03, 0x34, 0x12, 0x78, 0x56 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        services::GapAdvertisingDataParser::ListOf16BitUuids services;
        gapAdvertisingDataParser.CompleteListOf16BitUuids(services);

        ASSERT_EQ(2u, services.size());
        EXPECT_EQ(0x1234, services[0]);
        EXPECT_EQ(0x5678, services[1]);
    }

    TEST(GapAdvertisingDataParserTest, complete_list_of_16bit_services_at_an_odd_offset)
    {
        // The UUID list is preceded by a one-byte Flags structure, so its values start at offset 5.
        const std::array<uint8_t, 9> data{ { 0x02, 0x01, 0x06, 0x05, 0x03, 0x34, 0x12, 0x78, 0x56 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        services::GapAdvertisingDataParser::ListOf16BitUuids services;
        gapAdvertisingDataParser.CompleteListOf16BitUuids(services);

        ASSERT_EQ(2u, services.size());
        EXPECT_EQ(0x1234, services[0]);
        EXPECT_EQ(0x5678, services[1]);
    }

    TEST(GapAdvertisingDataParserTest, invalid_list_of_16bit_services)
    {
        const std::array<uint8_t, 5> data{ { 0x04, 0x03, 0x34, 0x12, 0x78 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        services::GapAdvertisingDataParser::ListOf16BitUuids services;
        gapAdvertisingDataParser.CompleteListOf16BitUuids(services);

        EXPECT_TRUE(services.empty());
    }

    TEST(GapAdvertisingDataParserTest, complete_list_of_128bit_services)
    {
        const std::array<uint8_t, 22> data{ { 0x11, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f } };
        const std::array<uint8_t, 16> service1{ { 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        services::GapAdvertisingDataParser::ListOf128BitUuids services;
        gapAdvertisingDataParser.CompleteListOf128BitUuids(services);

        ASSERT_EQ(1u, services.size());
        std::array<uint8_t, 16> parsedUuid = services[0];
        EXPECT_THAT(parsedUuid, testing::ContainerEq(service1));
    }

    TEST(GapAdvertisingDataParserTest, invalid_list_of_128bit_services)
    {
        const std::array<uint8_t, 21> data{ { 0x10, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        services::GapAdvertisingDataParser::ListOf128BitUuids services;
        gapAdvertisingDataParser.CompleteListOf128BitUuids(services);

        EXPECT_TRUE(services.empty());
    }

    TEST(GapAdvertisingDataParserTest, incomplete_list_of_16bit_services)
    {
        const std::array<uint8_t, 6> data{ { 0x05, 0x02, 0x34, 0x12, 0x78, 0x56 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        services::GapAdvertisingDataParser::ListOf16BitUuids services;
        gapAdvertisingDataParser.IncompleteListOf16BitUuids(services);

        EXPECT_THAT(services, testing::ElementsAre(0x1234, 0x5678));

        gapAdvertisingDataParser.CompleteListOf16BitUuids(services);
        EXPECT_TRUE(services.empty());
    }

    TEST(GapAdvertisingDataParserTest, incomplete_list_of_128bit_services)
    {
        const std::array<uint8_t, 18> data{ { 0x11, 0x06, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f } };
        const std::array<uint8_t, 16> service1{ { 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        services::GapAdvertisingDataParser::ListOf128BitUuids services;
        gapAdvertisingDataParser.IncompleteListOf128BitUuids(services);

        ASSERT_EQ(1u, services.size());
        std::array<uint8_t, 16> parsedUuid = services[0];
        EXPECT_THAT(parsedUuid, testing::ContainerEq(service1));
    }

    TEST(GapAdvertisingDataParserTest, service_data_128bit_uuid)
    {
        const std::array<uint8_t, 20> data{ { 0x13, 0x21, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0xAA, 0xBB } };
        const std::array<uint8_t, 16> expectedUuid{ { 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 } };
        const std::array<uint8_t, 2> expectedData{ { 0xAA, 0xBB } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));

        auto serviceData = gapAdvertisingDataParser.ServiceData128BitUuid();

        ASSERT_TRUE(serviceData);
        std::array<uint8_t, 16> parsedUuid = serviceData->first;
        EXPECT_THAT(parsedUuid, testing::ContainerEq(expectedUuid));
        EXPECT_THAT(serviceData->second, infra::ContentsEqual(expectedData));
    }

    TEST(GapAdvertisingDataParserTest, service_data_128bit_uuid_without_a_uuid)
    {
        const std::array<uint8_t, 4> data{ { 0x03, 0x21, 0x00, 0x01 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));

        EXPECT_FALSE(gapAdvertisingDataParser.ServiceData128BitUuid());
    }

    TEST(GapAdvertisingDataParserTest, flags_not_present)
    {
        const std::array<uint8_t, 2> data{ { 0x00, 0x00 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_FALSE(gapAdvertisingDataParser.Flags());
    }

    TEST(GapAdvertisingDataParserTest, flags_present)
    {
        const std::array<uint8_t, 3> data{ { 0x02, 0x01, 0x06 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));

        EXPECT_TRUE(gapAdvertisingDataParser.Flags());
        EXPECT_EQ(GapAdvertisementFlags::leGeneralDiscoverableMode | GapAdvertisementFlags::brEdrNotSupported, *gapAdvertisingDataParser.Flags());
    }

    TEST(GapAdvertisingDataParserTest, tx_power_level)
    {
        // TX Power Level is signed dBm: -6 on air is 0xFA.
        const std::array<uint8_t, 3> data{ { 0x02, 0x0A, 0xFA } };
        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        auto txPower = parser.TxPowerLevel();

        ASSERT_TRUE(txPower);
        EXPECT_EQ(-6, *txPower);
    }

    TEST(GapAdvertisingDataParserTest, absent_tx_power_level)
    {
        const std::array<uint8_t, 3> data{ { 0x02, 0x01, 0x06 } };
        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        EXPECT_FALSE(parser.TxPowerLevel());
    }

    TEST(GapAdvertisingDataParserTest, service_data_for_a_16_bit_uuid)
    {
        // Flags, then Service Data for UUID 0x180F carrying two bytes.
        const std::array<uint8_t, 8> data{ { 0x02, 0x01, 0x06, 0x04, 0x16, 0x0F, 0x18, 0x63 } };
        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        auto serviceData = parser.ServiceData16BitUuid();

        ASSERT_TRUE(serviceData);
        EXPECT_EQ(0x180F, serviceData->first);
        ASSERT_EQ(1u, serviceData->second.size());
        EXPECT_EQ(0x63, serviceData->second[0]);
    }

    TEST(GapAdvertisingDataParserTest, service_data_uuid_is_decoded_little_endian)
    {
        const std::array<uint8_t, 5> data{ { 0x04, 0x16, 0x34, 0x12, 0xAA } };
        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        auto serviceData = parser.ServiceData16BitUuid();

        ASSERT_TRUE(serviceData);
        EXPECT_EQ(0x1234, serviceData->first);
    }

    TEST(GapAdvertisingDataParserTest, service_data_too_short_to_hold_a_uuid)
    {
        const std::array<uint8_t, 3> data{ { 0x02, 0x16, 0x0F } };
        services::GapAdvertisingDataParser parser(infra::MakeConstByteRange(data));

        EXPECT_FALSE(parser.ServiceData16BitUuid());
    }
}
