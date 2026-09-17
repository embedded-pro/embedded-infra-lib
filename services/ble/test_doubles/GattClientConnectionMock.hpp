#ifndef SERVICES_GATT_CLIENT_CONNECTION_MOCK_HPP
#define SERVICES_GATT_CLIENT_CONNECTION_MOCK_HPP

#include "services/ble/GattClientCharacteristic.hpp"
#include "services/ble/GattClientConnection.hpp"
#include "gmock/gmock.h"

namespace services
{
    class GattClientConnectionMock
        : public GattClientConnection
    {
    public:
        MOCK_METHOD(uint16_t, EffectiveMaxAttMtuSize, (), (const override));
        MOCK_METHOD(void, ExchangeMtu, (), (override));

        MOCK_METHOD(void, StartServiceDiscovery, (), (override));
        MOCK_METHOD(void, StartCharacteristicDiscovery, (AttAttribute::Handle handle, AttAttribute::Handle endHandle), (override));
        MOCK_METHOD(void, StartDescriptorDiscovery, (AttAttribute::Handle handle, AttAttribute::Handle endHandle), (override));

        MOCK_METHOD(void, Read, (AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone), (override));
        MOCK_METHOD(void, Write, (AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone), (override));
        MOCK_METHOD(void, WriteWithoutResponse, (AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone), (override));

        MOCK_METHOD(void, EnableNotification, (AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone), (override));
        MOCK_METHOD(void, DisableNotification, (AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone), (override));
        MOCK_METHOD(void, EnableIndication, (AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone), (override));
        MOCK_METHOD(void, DisableIndication, (AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone), (override));
    };

    class GattClientConnectionObserverMock
        : public GattClientConnectionObserver
    {
    public:
        using GattClientConnectionObserver::GattClientConnectionObserver;

        MOCK_METHOD(void, ServiceDiscovered, (const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle), (override));
        MOCK_METHOD(void, ServiceDiscoveryComplete, (), (override));
        MOCK_METHOD(void, CharacteristicDiscovered, (const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties), (override));
        MOCK_METHOD(void, CharacteristicDiscoveryComplete, (), (override));
        MOCK_METHOD(void, DescriptorDiscovered, (const AttAttribute::Uuid& type, AttAttribute::Handle handle), (override));
        MOCK_METHOD(void, DescriptorDiscoveryComplete, (), (override));
        MOCK_METHOD(void, MtuChanged, (), (override));
    };

    class GattClientUpdateObserverMock
        : public GattClientUpdateObserver
    {
    public:
        using GattClientUpdateObserver::GattClientUpdateObserver;

        MOCK_METHOD(void, NotificationReceived, (AttAttribute::Handle handle, infra::ConstByteRange data), (override));
        MOCK_METHOD(void, IndicationReceived, (AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone), (override));
    };

    class GattClientCharacteristicUpdateObserverMock
        : public GattClientCharacteristicUpdateObserver
    {
    public:
        using GattClientCharacteristicUpdateObserver::GattClientCharacteristicUpdateObserver;

        MOCK_METHOD(void, NotificationReceived, (infra::ConstByteRange data), (override));
        MOCK_METHOD(void, IndicationReceived, (infra::ConstByteRange data, const infra::Function<void()>& onDone), (override));
    };
}

#endif
