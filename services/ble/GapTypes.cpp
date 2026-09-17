#include "services/ble/GapTypes.hpp"

namespace infra
{
    TextOutputStream& operator<<(TextOutputStream& stream, const services::GapAdvertisingEventType& eventType)
    {
        if (eventType == services::GapAdvertisingEventType::advInd)
            stream << "Advertising Indication";
        else if (eventType == services::GapAdvertisingEventType::advDirectInd)
            stream << "Directed Advertising Indication";
        else if (eventType == services::GapAdvertisingEventType::advScanInd)
            stream << "Scannable Advertising Indication";
        else if (eventType == services::GapAdvertisingEventType::scanResponse)
            stream << "Scan Response";
        else
            stream << "Non Connectable Advertising Indication";

        return stream;
    }

    TextOutputStream& operator<<(TextOutputStream& stream, const services::GapDeviceAddressType& addressType)
    {
        if (addressType == services::GapDeviceAddressType::publicAddress)
            stream << "Public Device Address";
        else
            stream << "Random Device Address";

        return stream;
    }
}
