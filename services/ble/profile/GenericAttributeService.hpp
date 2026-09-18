#ifndef SERVICES_GENERIC_ATTRIBUTE_SERVICE_HPP
#define SERVICES_GENERIC_ATTRIBUTE_SERVICE_HPP

#include "services/ble/GattServerCharacteristicImpl.hpp"
#include <array>

namespace services
{
    class GenericAttributeService
    {
    public:
        static constexpr uint16_t serviceChangedValueLength = 2 * sizeof(AttAttribute::Handle);

        explicit GenericAttributeService(GattServer& gattServer);

        void ServiceChanged(const GattServiceChanged& range);
        void ServiceChanged(AttAttribute::Handle startHandle, AttAttribute::Handle endHandle);

        GattServerService& Service();

    private:
        GattServerService service{ uuid::genericAttributeService };
        GattServerCharacteristicImpl serviceChanged{ service, uuid::serviceChanged, serviceChangedValueLength, GattCharacteristic::PropertyFlags::indicate };
        std::array<uint8_t, serviceChangedValueLength> value{};
    };
}

#endif
