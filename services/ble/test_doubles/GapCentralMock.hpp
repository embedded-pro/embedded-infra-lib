#ifndef SERVICES_GAP_CENTRAL_MOCK_HPP
#define SERVICES_GAP_CENTRAL_MOCK_HPP

#include "services/ble/GapCentral.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GapCentralMock
        : public GapCentral
    {
    public:
        MOCK_METHOD(std::optional<GapAddress>, ResolvePrivateAddress, (hal::MacAddress address), (const, override));
        using GapCentral::Connect;
        MOCK_METHOD(GapRequestStatus, Connect, (const GapAddress& peer, const GapConnectionParameters& parameters, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, UpdateConnectionParameters, (const GapConnectionParameters& parameters, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, CancelConnect, (const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, Disconnect, (const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetAddress, (const GapAddress& address, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetDataLength, (const GapDataLength& dataLength, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, SetPhy, (GapPhy txPhy, GapPhy rxPhy, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, StartDeviceDiscovery, (const GapScanParameters& parameters, const infra::Function<void(Result)>& onDone), (override));
        MOCK_METHOD(GapRequestStatus, StopDeviceDiscovery, (const infra::Function<void(Result)>& onDone), (override));

        void ChangeState(GapCentralState newState)
        {
            NotifyObservers([newState](auto& observer)
                {
                    observer.StateChanged(newState);
                });
        }
    };
}

#endif
