#ifndef SERVICES_GATT_CLIENT_CONNECTION_HPP
#define SERVICES_GATT_CLIENT_CONNECTION_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GattTypes.hpp"

namespace services
{
    enum class OperationStatus : uint8_t
    {
        success,
        retry,
        error
    };

    class GattClientConnection;

    class GattClientConnectionObserver
        : public infra::Observer<GattClientConnectionObserver, GattClientConnection>
    {
    public:
        using infra::Observer<GattClientConnectionObserver, GattClientConnection>::Observer;

        virtual void ServiceDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle) = 0;
        virtual void ServiceDiscoveryComplete() = 0;
        virtual void CharacteristicDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties) = 0;
        virtual void CharacteristicDiscoveryComplete() = 0;
        virtual void DescriptorDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle) = 0;
        virtual void DescriptorDiscoveryComplete() = 0;
        virtual void MtuChanged() = 0;
    };

    class GattClientUpdateObserver
        : public infra::Observer<GattClientUpdateObserver, GattClientConnection>
    {
    public:
        using infra::Observer<GattClientUpdateObserver, GattClientConnection>::Observer;

        virtual void NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data) = 0;
        virtual void IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
    };

    class GattClientConnection
        : public infra::Subject<GattClientConnectionObserver>
        , public infra::Subject<GattClientUpdateObserver>
    {
    public:
        virtual uint16_t EffectiveMaxAttMtuSize() const = 0;
        virtual void ExchangeMtu() = 0;

        virtual void StartServiceDiscovery() = 0;
        virtual void StartCharacteristicDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) = 0;
        virtual void StartDescriptorDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) = 0;

        void StartCharacteristicDiscovery(const GattService& service);
        void StartDescriptorDiscovery(const GattService& service);

        virtual void Read(AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone) = 0;
        virtual void Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone) = 0;
        virtual void WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone) = 0;

        virtual void EnableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) = 0;
        virtual void DisableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) = 0;
        virtual void EnableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) = 0;
        virtual void DisableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) = 0;
    };

    class GattClientConnectionDecorator
        : public GattClientConnectionObserver
        , public GattClientUpdateObserver
        , public GattClientConnection
    {
    public:
        explicit GattClientConnectionDecorator(GattClientConnection& connection);

        // Implementation of GattClientConnectionObserver
        void ServiceDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle) override;
        void ServiceDiscoveryComplete() override;
        void CharacteristicDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties) override;
        void CharacteristicDiscoveryComplete() override;
        void DescriptorDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle) override;
        void DescriptorDiscoveryComplete() override;
        void MtuChanged() override;

        // Implementation of GattClientUpdateObserver
        void NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data) override;
        void IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

        // Implementation of GattClientConnection
        uint16_t EffectiveMaxAttMtuSize() const override;
        void ExchangeMtu() override;
        void StartServiceDiscovery() override;
        void StartCharacteristicDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) override;
        void StartDescriptorDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) override;
        void Read(AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone) override;
        void Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone) override;
        void WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone) override;
        void EnableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void DisableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void EnableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void DisableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
    };
}

#endif
