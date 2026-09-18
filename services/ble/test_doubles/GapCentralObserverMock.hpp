#ifndef SERVICES_GAP_CENTRAL_OBSERVER_MOCK_HPP
#define SERVICES_GAP_CENTRAL_OBSERVER_MOCK_HPP

#include "services/ble/GapCentral.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapCentralObserverMock
        : public GapCentralObserver
    {
        using GapCentralObserver::GapCentralObserver;

    public:
        MOCK_METHOD(void, DeviceDiscovered, (const GapAdvertisingReport& deviceDiscovered), (override));
        MOCK_METHOD(void, StateChanged, (GapCentralState state), (override));
        MOCK_METHOD(void, PhyUpdated, (GapPhy txPhy, GapPhy rxPhy), (override));
        MOCK_METHOD(void, DataLengthChanged, (const GapDataLength& dataLength), (override));
    };
}

#endif
