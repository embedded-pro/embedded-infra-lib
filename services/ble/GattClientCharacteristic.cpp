#include "services/ble/GattClientCharacteristic.hpp"

namespace services
{
    GattClientCharacteristic::GattClientCharacteristic(GattClientConnection& connection, AttAttribute::Uuid type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties)
        : GattCharacteristic(type, handle, valueHandle, properties)
        , GattClientUpdateObserver(connection)
    {}

    GattRequestStatus GattClientCharacteristic::Read(const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        return GattClientUpdateObserver::Subject().Read(ValueHandle(), onDone);
    }

    GattRequestStatus GattClientCharacteristic::Write(infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().Write(ValueHandle(), data, onDone);
    }

    GattRequestStatus GattClientCharacteristic::WriteWithoutResponse(infra::ConstByteRange data)
    {
        return GattClientUpdateObserver::Subject().WriteWithoutResponse(ValueHandle(), data);
    }

    GattRequestStatus GattClientCharacteristic::EnableNotification(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().EnableNotification(ValueHandle(), onDone);
    }

    GattRequestStatus GattClientCharacteristic::DisableNotification(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().DisableNotification(ValueHandle(), onDone);
    }

    GattRequestStatus GattClientCharacteristic::EnableIndication(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().EnableIndication(ValueHandle(), onDone);
    }

    GattRequestStatus GattClientCharacteristic::DisableIndication(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().DisableIndication(ValueHandle(), onDone);
    }

    void GattClientCharacteristic::NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        if (handle == ValueHandle())
            GattClientCharacteristicUpdate::SubjectType::NotifyObservers([&data](auto& obs)
                {
                    obs.NotificationReceived(data);
                });
    }

    void GattClientCharacteristic::IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        if (handle == ValueHandle())
        {
            onIndicationDone = onDone;
            observers = 1;
            auto indicationReceived = [this]()
            {
                --observers;
                if (observers == 0)
                    onIndicationDone();
            };

            GattClientCharacteristicUpdate::SubjectType::NotifyObservers([this, &data, &indicationReceived](auto& obs)
                {
                    ++observers;
                    obs.IndicationReceived(data, indicationReceived);
                });

            indicationReceived();
        }
        else
            onDone();
    }

    AttAttribute::Handle GattClientCharacteristic::CharacteristicValueHandle() const
    {
        return ValueHandle();
    }

    GattCharacteristic::PropertyFlags GattClientCharacteristic::CharacteristicProperties() const
    {
        return Properties();
    }

    GattClientService::GattClientService(const AttAttribute::Uuid& type, const AttAttribute::Handle& handle, const AttAttribute::Handle& endHandle)
        : GattService(type, handle, endHandle)
    {}

    void GattClientService::AddCharacteristic(GattClientCharacteristic& characteristic)
    {
        characteristics.push_front(characteristic);
    }

    const infra::IntrusiveForwardList<GattClientCharacteristic>& GattClientService::Characteristics() const
    {
        return characteristics;
    }
}
