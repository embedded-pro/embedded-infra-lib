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
        auto services = gapAdvertisingDataParser.CompleteListOf16BitUuids();

        ASSERT_EQ(2u, services.size());
        EXPECT_EQ(0x1234, services[0]);
        EXPECT_EQ(0x5678, services[1]);
    }

    TEST(GapAdvertisingDataParserTest, invalid_list_of_16bit_services)
    {
        const std::array<uint8_t, 5> data{ { 0x04, 0x03, 0x34, 0x12, 0x78 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeConstByteRange(data));
        auto services = gapAdvertisingDataParser.CompleteListOf16BitUuids();

        EXPECT_TRUE(services.empty());
    }

    TEST(GapAdvertisingDataParserTest, complete_list_of_128bit_services)
    {
        const std::array<uint8_t, 22> data{ { 0x11, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f } };
        const std::array<uint8_t, 16> service1{ { 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        auto services = gapAdvertisingDataParser.CompleteListOf128BitUuids();

        ASSERT_EQ(1u, services.size());
        std::array<uint8_t, 16> parsedUuid = services[0];
        EXPECT_THAT(parsedUuid, testing::ContainerEq(service1));
    }

    TEST(GapAdvertisingDataParserTest, invalid_list_of_128bit_services)
    {
        const std::array<uint8_t, 21> data{ { 0x10, 0x07, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e } };
        services::GapAdvertisingDataParser gapAdvertisingDataParser(infra::MakeRange(data));
        auto services = gapAdvertisingDataParser.CompleteListOf128BitUuids();

        EXPECT_TRUE(services.empty());
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
}
