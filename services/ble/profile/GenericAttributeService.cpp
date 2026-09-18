#include "services/ble/profile/GenericAttributeService.hpp"
#include "infra/stream/ByteOutputStream.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace services
{
    GenericAttributeService::GenericAttributeService(GattServer& gattServer)
    {
        gattServer.AddService(service);
    }

    void GenericAttributeService::ServiceChanged(const GattServiceChanged& range)
    {
        really_assert(range.startHandle <= range.endHandle);

        infra::ByteOutputStream stream(infra::MakeRange(value));
        stream << infra::ToLittleEndian(range.startHandle) << infra::ToLittleEndian(range.endHandle);

        serviceChanged.Update(infra::MakeConstByteRange(value), []() {});
    }

    void GenericAttributeService::ServiceChanged(AttAttribute::Handle startHandle, AttAttribute::Handle endHandle)
    {
        ServiceChanged(GattServiceChanged{ startHandle, endHandle });
    }

    GattServerService& GenericAttributeService::Service()
    {
        return service;
    }
}
