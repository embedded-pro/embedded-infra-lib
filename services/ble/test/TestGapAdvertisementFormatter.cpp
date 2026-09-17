#include "infra/util/BoundedString.hpp"
#include "infra/util/test_helper/MemoryRangeMatcher.hpp"
#include "services/ble/GapAdvertisingData.hpp"
#include "gmock/gmock.h"

namespace services
{
    namespace
    {
        class GapAdvertisementFormatterTest
            : public testing::Test
        {
        public:
            infra::BoundedVector<uint8_t>::WithMaxSize<services::gapMaxScanResponseDataSize> buffer;
            GapAdvertisementFormatter formatter{ buffer };
        };
    }

    TEST_F(GapAdvertisementFormatterTest, initial_state_is_empty)
    {
        EXPECT_THAT(formatter.FormattedAdvertisementData(), testing::IsEmpty());
        EXPECT_EQ(formatter.RemainingSpaceAvailable(), gapMaxScanResponseDataSize);
    }

    TEST_F(GapAdvertisementFormatterTest, append_flags)
    {
        formatter.AppendFlags(GapAdvertisementFlags::leGeneralDiscoverableMode);

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 3);
        EXPECT_EQ(data[0], 2);
        EXPECT_EQ(data[1], 0x01);
        EXPECT_EQ(data[2], static_cast<uint8_t>(GapAdvertisementFlags::leGeneralDiscoverableMode));

        EXPECT_EQ(formatter.RemainingSpaceAvailable(), gapMaxScanResponseDataSize - 3);
    }

    TEST_F(GapAdvertisementFormatterTest, append_complete_local_name)
    {
        infra::BoundedConstString name{ "Test" };
        formatter.AppendCompleteLocalName(name);

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 6);
        EXPECT_EQ(data[0], 5);
        EXPECT_EQ(data[1], 0x09);
        EXPECT_THAT(infra::MakeRange(data.begin() + 2, data.end()), infra::ContentsEqual(name));
    }

    TEST_F(GapAdvertisementFormatterTest, append_shortened_local_name)
    {
        infra::BoundedConstString name{ "Test" };
        formatter.AppendShortenedLocalName(name);

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 6);
        EXPECT_EQ(data[0], 5);
        EXPECT_EQ(data[1], 0x08);
        EXPECT_THAT(infra::MakeRange(data.begin() + 2, data.end()), infra::ContentsEqual(name));
    }

    TEST_F(GapAdvertisementFormatterTest, append_manufacturer_data)
    {
        uint16_t manufacturerCode = 0x1234;
        std::array<uint8_t, 3> manufacturerData = { 0xAB, 0xCD, 0xEF };

        formatter.AppendManufacturerData(manufacturerCode, infra::MakeByteRange(manufacturerData));

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 7);
        EXPECT_EQ(data[0], 6);
        EXPECT_EQ(data[1], 0xFF);
        EXPECT_EQ(data[2], 0x34);
        EXPECT_EQ(data[3], 0x12);
        EXPECT_EQ(data[4], 0xAB);
        EXPECT_EQ(data[5], 0xCD);
        EXPECT_EQ(data[6], 0xEF);
    }

    TEST_F(GapAdvertisementFormatterTest, append_list_of_16bit_services)
    {
        std::array<AttAttribute::Uuid16, 2> services = { 0x1234, 0x5678 };

        formatter.AppendListOfServicesUuid(infra::MakeRange(services));

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 6);
        EXPECT_EQ(data[0], 5);
        EXPECT_EQ(data[1], 0x03);
        EXPECT_EQ(data[2], 0x34);
        EXPECT_EQ(data[3], 0x12);
        EXPECT_EQ(data[4], 0x78);
        EXPECT_EQ(data[5], 0x56);
    }

    TEST_F(GapAdvertisementFormatterTest, append_list_of_128bit_services)
    {
        std::array<uint8_t, 16> uuid128_1 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
        AttAttribute::Uuid128 service1(uuid128_1);
        std::array<AttAttribute::Uuid128, 1> services = { service1 };

        formatter.AppendListOfServicesUuid(infra::MakeRange(services));

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 18);
        EXPECT_EQ(data[0], 17);
        EXPECT_EQ(data[1], 0x07);

        const std::array<uint8_t, 16> onAir = { 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
            0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
        EXPECT_THAT(infra::MakeRange(data.begin() + 2, data.end()), infra::ContentsEqual(onAir));
    }

    TEST_F(GapAdvertisementFormatterTest, append_incomplete_list_of_16bit_services)
    {
        std::array<AttAttribute::Uuid16, 1> services = { 0x1234 };

        formatter.AppendIncompleteListOfServicesUuid(infra::MakeRange(services));

        auto data = formatter.FormattedAdvertisementData();
        ASSERT_EQ(4u, data.size());
        EXPECT_EQ(3, data[0]);
        EXPECT_EQ(0x02, data[1]);
        EXPECT_EQ(0x34, data[2]);
        EXPECT_EQ(0x12, data[3]);
    }

    TEST_F(GapAdvertisementFormatterTest, append_incomplete_list_of_128bit_services)
    {
        std::array<uint8_t, 16> uuid128 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
        std::array<AttAttribute::Uuid128, 1> services = { AttAttribute::Uuid128(uuid128) };

        formatter.AppendIncompleteListOfServicesUuid(infra::MakeRange(services));

        auto data = formatter.FormattedAdvertisementData();
        ASSERT_EQ(18u, data.size());
        EXPECT_EQ(17, data[0]);
        EXPECT_EQ(0x06, data[1]);

        const std::array<uint8_t, 16> onAir = { 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
            0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };
        EXPECT_THAT(infra::MakeRange(data.begin() + 2, data.end()), infra::ContentsEqual(onAir));
    }

    TEST_F(GapAdvertisementFormatterTest, append_service_data_with_a_128bit_uuid)
    {
        std::array<uint8_t, 16> uuid128 = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };
        const std::array<uint8_t, 2> serviceData = { 0xAA, 0xBB };

        formatter.AppendServiceData(AttAttribute::Uuid128(uuid128), infra::MakeRange(serviceData));

        auto data = formatter.FormattedAdvertisementData();
        ASSERT_EQ(20u, data.size());
        EXPECT_EQ(19, data[0]);
        EXPECT_EQ(0x21, data[1]);

        const std::array<uint8_t, 18> expected = { 0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08,
            0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, 0xAA, 0xBB };
        EXPECT_THAT(infra::MakeRange(data.begin() + 2, data.end()), infra::ContentsEqual(expected));
    }

    TEST_F(GapAdvertisementFormatterTest, append_public_target_address)
    {
        hal::MacAddress address({ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });

        formatter.AppendPublicTargetAddress(address);

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 8);
        EXPECT_EQ(data[0], 7);
        EXPECT_EQ(data[1], 0x17);
        EXPECT_EQ(data[2], 0x01);
        EXPECT_EQ(data[3], 0x02);
        EXPECT_EQ(data[4], 0x03);
        EXPECT_EQ(data[5], 0x04);
        EXPECT_EQ(data[6], 0x05);
        EXPECT_EQ(data[7], 0x06);
    }

    TEST_F(GapAdvertisementFormatterTest, multiple_append_operations)
    {
        formatter.AppendFlags(GapAdvertisementFlags::leGeneralDiscoverableMode);

        infra::BoundedConstString name{ "Test" };
        formatter.AppendCompleteLocalName(name);

        std::array<AttAttribute::Uuid16, 1> services = { 0x1234 };
        formatter.AppendListOfServicesUuid(infra::MakeRange(services));

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 13);

        EXPECT_EQ(data[0], 2);
        EXPECT_EQ(data[1], 0x01);
        EXPECT_EQ(data[2], static_cast<uint8_t>(GapAdvertisementFlags::leGeneralDiscoverableMode));

        EXPECT_EQ(data[3], 5);
        EXPECT_EQ(data[4], 0x09);

        EXPECT_EQ(data[9], 3);
        EXPECT_EQ(data[10], 0x03);
    }

    TEST_F(GapAdvertisementFormatterTest, remaining_space_calculation)
    {
        std::size_t initialSpace = formatter.RemainingSpaceAvailable();
        EXPECT_EQ(initialSpace, gapMaxScanResponseDataSize);

        formatter.AppendFlags(GapAdvertisementFlags::leGeneralDiscoverableMode);
        EXPECT_EQ(formatter.RemainingSpaceAvailable(), initialSpace - 3);

        infra::BoundedConstString name{ "Test" };
        formatter.AppendCompleteLocalName(name);
        EXPECT_EQ(formatter.RemainingSpaceAvailable(), initialSpace - 3 - 6);
    }

    TEST_F(GapAdvertisementFormatterTest, edge_case_single_byte_manufacturer_data)
    {
        uint16_t manufacturerCode = 0x0001;
        std::array<uint8_t, 1> manufacturerData = { 0xFF };

        formatter.AppendManufacturerData(manufacturerCode, infra::MakeByteRange(manufacturerData));

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 5);
        EXPECT_EQ(data[0], 4);
        EXPECT_EQ(data[1], 0xFF);
        EXPECT_EQ(data[2], 0x01);
        EXPECT_EQ(data[3], 0x00);
        EXPECT_EQ(data[4], 0xFF);
    }

    TEST_F(GapAdvertisementFormatterTest, append_appearance)
    {
        uint16_t keyboardAppearance = 0x03C1;

        formatter.AppendAppearance(keyboardAppearance);

        auto data = formatter.FormattedAdvertisementData();
        EXPECT_EQ(data.size(), 4);
        EXPECT_EQ(data[0], 3);
        EXPECT_EQ(data[1], 0x19);
        EXPECT_EQ(data[2], 0xC1);
        EXPECT_EQ(data[3], 0x03);

        EXPECT_EQ(formatter.RemainingSpaceAvailable(), gapMaxScanResponseDataSize - 4);
    }

    TEST_F(GapAdvertisementFormatterTest, append_tx_power_level)
    {
        formatter.AppendTxPowerLevel(-6);

        auto data = formatter.FormattedAdvertisementData();
        ASSERT_EQ(3u, data.size());
        EXPECT_EQ(2, data[0]);
        EXPECT_EQ(0x0A, data[1]);
        EXPECT_EQ(0xFA, data[2]);
    }

    TEST_F(GapAdvertisementFormatterTest, append_service_data_for_a_16_bit_uuid)
    {
        const std::array<uint8_t, 2> serviceData{ 0x63, 0x64 };

        formatter.AppendServiceData(0x180F, infra::MakeConstByteRange(serviceData));

        auto data = formatter.FormattedAdvertisementData();
        ASSERT_EQ(6u, data.size());
        EXPECT_EQ(5, data[0]);
        EXPECT_EQ(0x16, data[1]);
        EXPECT_EQ(0x0F, data[2]); // little-endian UUID
        EXPECT_EQ(0x18, data[3]);
        EXPECT_EQ(0x63, data[4]);
        EXPECT_EQ(0x64, data[5]);
    }
}
