#ifndef SERVICES_GATT_SERVER_MOCK_HPP
#define SERVICES_GATT_SERVER_MOCK_HPP

#include "services/ble/GattServer.hpp"
#include "services/ble/GattTypes.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GattServerMock
        : public GattServer
    {
    public:
        MOCK_METHOD(void, AddService, (GattServerService & service), (override));
    };

    class GattServerCharacteristicOperationsMock
        : public services::GattServerCharacteristicOperations
    {
    public:
        MOCK_METHOD(GattRequestStatus, Update, (const services::GattServerCharacteristicOperationsObserver& characteristic, infra::ConstByteRange data), (const, override));
    };

    class GattServerCharacteristicUpdateMock
        : public GattServerCharacteristicUpdate
    {
    public:
        MOCK_METHOD(void, Update, (infra::ConstByteRange data, infra::Function<void()> onDone), (override));
    };

    class GattServerCharacteristicMock
        : public GattServerCharacteristic
    {
    public:
        MOCK_METHOD(AttAttribute::Handle, ServiceHandle, (), (const, override));
        MOCK_METHOD(AttAttribute::Handle, CharacteristicHandle, (), (const, override));
        MOCK_METHOD(void, Update, (infra::ConstByteRange data, infra::Function<void()> onDone), (override));
    };
}

#endif
