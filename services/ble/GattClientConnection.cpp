#include "services/ble/GattClientConnection.hpp"

namespace services
{
    void GattClientConnection::StartCharacteristicDiscovery(const GattService& service)
    {
        StartCharacteristicDiscovery(service.Handle(), service.EndHandle());
    }

    void GattClientConnection::StartDescriptorDiscovery(const GattService& service)
    {
        StartDescriptorDiscovery(service.Handle(), service.EndHandle());
    }

    GattClientConnectionDecorator::GattClientConnectionDecorator(GattClientConnection& connection)
        : GattClientConnectionObserver(connection)
        , GattClientUpdateObserver(connection)
    {}

    void GattClientConnectionDecorator::ServiceDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle endHandle)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&type, handle, endHandle](auto& observer)
            {
                observer.ServiceDiscovered(type, handle, endHandle);
            });
    }

    void GattClientConnectionDecorator::ServiceDiscoveryComplete()
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([](auto& observer)
            {
                observer.ServiceDiscoveryComplete();
            });
    }

    void GattClientConnectionDecorator::CharacteristicDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&type, handle, valueHandle, properties](auto& observer)
            {
                observer.CharacteristicDiscovered(type, handle, valueHandle, properties);
            });
    }

    void GattClientConnectionDecorator::CharacteristicDiscoveryComplete()
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([](auto& observer)
            {
                observer.CharacteristicDiscoveryComplete();
            });
    }

    void GattClientConnectionDecorator::DescriptorDiscovered(const AttAttribute::Uuid& type, AttAttribute::Handle handle)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&type, handle](auto& observer)
            {
                observer.DescriptorDiscovered(type, handle);
            });
    }

    void GattClientConnectionDecorator::DescriptorDiscoveryComplete()
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([](auto& observer)
            {
                observer.DescriptorDiscoveryComplete();
            });
    }

    void GattClientConnectionDecorator::MtuChanged()
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([](auto& observer)
            {
                observer.MtuChanged();
            });
    }

    void GattClientConnectionDecorator::NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        infra::Subject<GattClientUpdateObserver>::NotifyObservers([handle, data](auto& observer)
            {
                observer.NotificationReceived(handle, data);
            });
    }

    void GattClientConnectionDecorator::IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        infra::Subject<GattClientUpdateObserver>::NotifyObservers([handle, data, &onDone](auto& observer)
            {
                observer.IndicationReceived(handle, data, onDone);
            });
    }

    uint16_t GattClientConnectionDecorator::EffectiveMaxAttMtuSize() const
    {
        return GattClientConnectionObserver::Subject().EffectiveMaxAttMtuSize();
    }

    void GattClientConnectionDecorator::ExchangeMtu()
    {
        GattClientConnectionObserver::Subject().ExchangeMtu();
    }

    void GattClientConnectionDecorator::StartServiceDiscovery()
    {
        GattClientConnectionObserver::Subject().StartServiceDiscovery();
    }

    void GattClientConnectionDecorator::StartCharacteristicDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle)
    {
        GattClientConnectionObserver::Subject().StartCharacteristicDiscovery(handle, endHandle);
    }

    void GattClientConnectionDecorator::StartDescriptorDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle)
    {
        GattClientConnectionObserver::Subject().StartDescriptorDiscovery(handle, endHandle);
    }

    void GattClientConnectionDecorator::Read(AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().Read(handle, onRead, onDone);
    }

    void GattClientConnectionDecorator::Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().Write(handle, data, onDone);
    }

    void GattClientConnectionDecorator::WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone)
    {
        GattClientConnectionObserver::Subject().WriteWithoutResponse(handle, data, onDone);
    }

    void GattClientConnectionDecorator::EnableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().EnableNotification(handle, onDone);
    }

    void GattClientConnectionDecorator::DisableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().DisableNotification(handle, onDone);
    }

    void GattClientConnectionDecorator::EnableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().EnableIndication(handle, onDone);
    }

    void GattClientConnectionDecorator::DisableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientConnectionObserver::Subject().DisableIndication(handle, onDone);
    }
}
