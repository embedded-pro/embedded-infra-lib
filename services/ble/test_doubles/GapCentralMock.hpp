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
        MOCK_METHOD(std::optional<hal::MacAddress>, ResolvePrivateAddress, (hal::MacAddress address), (const));
        MOCK_METHOD(GapRequestStatus, Connect, (hal::MacAddress macAddress, GapDeviceAddressType addressType, infra::Duration initiatingTimeout, const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, CancelConnect, (const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, Disconnect, (const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, SetAddress, (hal::MacAddress macAddress, GapDeviceAddressType addressType, const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, StartDeviceDiscovery, (const infra::Function<void(Result)>& onDone));
        MOCK_METHOD(GapRequestStatus, StopDeviceDiscovery, (const infra::Function<void(Result)>& onDone));

        void ChangeState(GapState newState)
        {
            NotifyObservers([newState](auto& observer)
                {
                    observer.StateChanged(newState);
                });
        }
    };
}

#endif
