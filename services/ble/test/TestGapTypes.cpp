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

    TEST(GapInsertionOperatorEventAddressTypeTest, address_event_type_overload_operator)
    {
        infra::StringOutputStream::WithStorage<128> stream;

        stream << GapDeviceAddressType::publicAddress << " " << GapDeviceAddressType::randomAddress;

        EXPECT_EQ("Public Device Address Random Device Address", stream.Storage());
    }
}
