#include "services/ble/GattClientCharacteristic.hpp"

namespace services
{
    GattClientCharacteristic::GattClientCharacteristic(GattClientConnection& connection, AttAttribute::Uuid type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties)
        : GattCharacteristic(type, handle, valueHandle, properties)
        , GattClientUpdateObserver(connection)
    {}

    GattRequestStatus GattClientCharacteristic::Read(const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        return GattClientUpdateObserver::Subject().Read(valueHandle, onDone);
    }

    GattRequestStatus GattClientCharacteristic::Write(infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().Write(valueHandle, data, onDone);
    }

    GattRequestStatus GattClientCharacteristic::WriteWithoutResponse(infra::ConstByteRange data)
    {
        return GattClientUpdateObserver::Subject().WriteWithoutResponse(valueHandle, data);
    }

    GattRequestStatus GattClientCharacteristic::EnableNotification(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().EnableNotification(valueHandle, onDone);
    }

    GattRequestStatus GattClientCharacteristic::DisableNotification(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().DisableNotification(valueHandle, onDone);
    }

    GattRequestStatus GattClientCharacteristic::EnableIndication(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().EnableIndication(valueHandle, onDone);
    }

    GattRequestStatus GattClientCharacteristic::DisableIndication(const infra::Function<void(GattResult)>& onDone)
    {
        return GattClientUpdateObserver::Subject().DisableIndication(valueHandle, onDone);
    }

    void GattClientCharacteristic::NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        if (handle == (Handle() + GattDescriptor::ClientCharacteristicConfiguration::valueHandleOffset))
            GattClientCharacteristicUpdate::SubjectType::NotifyObservers([&data](auto& obs)
                {
                    obs.NotificationReceived(data);
                });
    }

    void GattClientCharacteristic::IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        if (handle == (Handle() + GattDescriptor::ClientCharacteristicConfiguration::valueHandleOffset))
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
        return valueHandle;
    }

    GattCharacteristic::PropertyFlags GattClientCharacteristic::CharacteristicProperties() const
    {
        return properties;
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
