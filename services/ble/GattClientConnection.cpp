#include "services/ble/GattClientConnection.hpp"

namespace services
{
    GattRequestStatus GattClientConnection::DiscoverCharacteristics(const GattService& service, const infra::Function<void(GattResult)>& onDone)
    {
        return DiscoverCharacteristics(service.Handle(), service.EndHandle(), onDone);
    }

    GattRequestStatus GattClientConnection::DiscoverDescriptors(const GattService& service, const infra::Function<void(GattResult)>& onDone)
    {
        return DiscoverDescriptors(service.Handle(), service.EndHandle(), onDone);
    }

    void GattIndicationFanOut::Deliver(infra::Subject<GattClientUpdateObserver>& observers, AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        this->onDone = onDone;
        outstanding = 1;
        auto handled = [this]()
        {
            Handled();
        };

        observers.NotifyObservers([this, handle, data, &handled](auto& observer)
            {
                ++outstanding;
                observer.IndicationReceived(handle, data, handled);
            });

        Handled();
    }

    void GattIndicationFanOut::Handled()
    {
        --outstanding;
        if (outstanding == 0)
            onDone();
    }

    GattClientConnectionDecorator::GattClientConnectionDecorator(GattClientConnection& connection)
        : GattClientConnectionObserver(connection)
        , GattClientUpdateObserver(connection)
    {}

    void GattClientConnectionDecorator::ServiceDiscovered(const GattService& service)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&service](auto& observer)
            {
                observer.ServiceDiscovered(service);
            });
    }

    void GattClientConnectionDecorator::CharacteristicDiscovered(const GattCharacteristic& characteristic)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&characteristic](auto& observer)
            {
                observer.CharacteristicDiscovered(characteristic);
            });
    }

    void GattClientConnectionDecorator::DescriptorDiscovered(const GattDescriptor& descriptor)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([&descriptor](auto& observer)
            {
                observer.DescriptorDiscovered(descriptor);
            });
    }

    void GattClientConnectionDecorator::MtuChanged(uint16_t mtu)
    {
        infra::Subject<GattClientConnectionObserver>::NotifyObservers([mtu](auto& observer)
            {
                observer.MtuChanged(mtu);
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
        indicationFanOut.Deliver(*this, handle, data, onDone);
    }

    uint16_t GattClientConnectionDecorator::EffectiveMaxAttMtuSize() const
    {
        return GattClientConnectionObserver::Subject().EffectiveMaxAttMtuSize();
    }

    GattRequestStatus GattClientConnectionDecorator::ExchangeMtu(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().ExchangeMtu(onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::DiscoverServices(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().DiscoverServices(onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().DiscoverCharacteristics(handle, endHandle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().DiscoverDescriptors(handle, endHandle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        return GattClientConnectionObserver::Subject().Read(handle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().Write(handle, data, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        return GattClientConnectionObserver::Subject().WriteWithoutResponse(handle, data);
    }

    GattRequestStatus GattClientConnectionDecorator::EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().EnableNotification(handle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().DisableNotification(handle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().EnableIndication(handle, onDone);
    }

    GattRequestStatus GattClientConnectionDecorator::DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientConnectionObserver::Subject().DisableIndication(handle, onDone);
    }
}
