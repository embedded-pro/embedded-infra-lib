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
        MOCK_METHOD(GattRequestStatus, ExchangeMtu, (const infra::Function<void(GattResult)>& onDone), (override));

        MOCK_METHOD(GattRequestStatus, DiscoverServices, (const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, DiscoverCharacteristics, (AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, DiscoverDescriptors, (AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone), (override));

        MOCK_METHOD(GattRequestStatus, Read, (AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, Write, (AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, WriteWithoutResponse, (AttAttribute::Handle handle, infra::ConstByteRange data), (override));
        MOCK_METHOD(GattRequestStatus, ReadBlob, (AttAttribute::Handle handle, uint16_t offset, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, PrepareWrite, (AttAttribute::Handle handle, uint16_t offset, infra::ConstByteRange data, const infra::Function<void(GattResult, uint16_t, infra::ConstByteRange)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, ExecuteWrite, (GattExecuteWriteFlag flag, const infra::Function<void(GattResult)>& onDone), (override));

        MOCK_METHOD(GattRequestStatus, EnableNotification, (AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, DisableNotification, (AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, EnableIndication, (AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone), (override));
        MOCK_METHOD(GattRequestStatus, DisableIndication, (AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone), (override));

        void NotifyIndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone)
        {
            indicationFanOut.Deliver(*this, handle, data, onDone);
        }

    private:
        GattIndicationFanOut indicationFanOut;
    };

    class GattClientConnectionObserverMock
        : public GattClientConnectionObserver
    {
    public:
        using GattClientConnectionObserver::GattClientConnectionObserver;

        MOCK_METHOD(void, ServiceDiscovered, (const GattService& service), (override));
        MOCK_METHOD(void, CharacteristicDiscovered, (const GattCharacteristic& characteristic), (override));
        MOCK_METHOD(void, DescriptorDiscovered, (const GattDescriptor& descriptor), (override));
        MOCK_METHOD(void, MtuChanged, (uint16_t mtu), (override));
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
