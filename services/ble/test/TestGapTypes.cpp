#include "infra/stream/StringOutputStream.hpp"
#include "services/ble/GapTypes.hpp"
#include "gmock/gmock.h"

namespace services
{
    TEST(GapInsertionOperatorEventTypeTest, event_type_overload_operator)
    {
        infra::StringOutputStream::WithStorage<256> stream;

        stream << GapAdvertisingEventType::advInd << " " << GapAdvertisingEventType::advDirectInd << " " << GapAdvertisingEventType::advScanInd << " " << GapAdvertisingEventType::advNonconnInd << " " << GapAdvertisingEventType::scanResponse;

        EXPECT_EQ("Advertising Indication Directed Advertising Indication Scannable Advertising Indication Non Connectable Advertising Indication Scan Response", stream.Storage());
    }

    TEST(GapConnectionParametersTest, documents_the_specification_ranges)
    {
        // Bluetooth Core Specification, Volume 4, Part E, section 7.8.12.
        EXPECT_EQ(0x0006u, GapConnectionParameters::connectionIntervalMultiplierMin);
        EXPECT_EQ(0x0C80u, GapConnectionParameters::connectionIntervalMultiplierMax);
        EXPECT_EQ(0x000Au, GapConnectionParameters::supervisionTimeoutMultiplierMin);
        EXPECT_EQ(0x0C80u, GapConnectionParameters::supervisionTimeoutMultiplierMax);
    }

    TEST(GapDataLengthTest, tx_time_depends_on_the_phy)
    {
        EXPECT_EQ(251u, GapDataLength::initialMaxTxOctets);

        EXPECT_EQ(2120u, GapDataLength::InitialMaxTxTime(GapPhy::le1M));

        // LE 2M halves the time per octet and spends one octet more on its preamble, so it is
        // neither the 1M value nor exactly half of it.
        EXPECT_EQ(1064u, GapDataLength::InitialMaxTxTime(GapPhy::le2M));
        EXPECT_EQ(17040u, GapDataLength::InitialMaxTxTime(GapPhy::leCoded));
    }

    TEST(GapDataLengthTest, maximum_carries_the_longest_payload_for_the_phy)
    {
        EXPECT_EQ((GapDataLength{ 251u, 2120u }), GapDataLength::Maximum(GapPhy::le1M));
        EXPECT_EQ((GapDataLength{ 251u, 1064u }), GapDataLength::Maximum(GapPhy::le2M));
        EXPECT_EQ((GapDataLength{ 251u, 17040u }), GapDataLength::Maximum(GapPhy::leCoded));
    }

    TEST(GapInsertionOperatorPhyTest, phy_overload_operator)
    {
        infra::StringOutputStream::WithStorage<128> stream;

        stream << GapPhy::le1M << " " << GapPhy::le2M << " " << GapPhy::leCoded;

        EXPECT_EQ("LE 1M LE 2M LE Coded", stream.Storage());
    }

    TEST(GapInsertionOperatorEventAddressTypeTest, address_event_type_overload_operator)
    {
        infra::StringOutputStream::WithStorage<128> stream;

        stream << GapDeviceAddressType::publicAddress << " " << GapDeviceAddressType::randomAddress;

        EXPECT_EQ("Public Device Address Random Device Address", stream.Storage());
    }
}
