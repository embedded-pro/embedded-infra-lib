#include "services/ble/GattClientCharacteristic.hpp"

namespace services
{
    GattClientCharacteristic::GattClientCharacteristic(GattClientConnection& connection, AttAttribute::Uuid type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties)
        : GattCharacteristic(type, handle, valueHandle, properties)
        , GattClientUpdateObserver(connection)
    {}

    void GattClientCharacteristic::Read(const infra::Function<void(const infra::ConstByteRange&)>& onResponse, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().Read(valueHandle, onResponse, onDone);
    }

    void GattClientCharacteristic::Write(infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().Write(valueHandle, data, onDone);
    }

    void GattClientCharacteristic::WriteWithoutResponse(infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone)
    {
        GattClientUpdateObserver::Subject().WriteWithoutResponse(valueHandle, data, onDone);
    }

    void GattClientCharacteristic::EnableNotification(const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().EnableNotification(valueHandle, onDone);
    }

    void GattClientCharacteristic::DisableNotification(const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().DisableNotification(valueHandle, onDone);
    }

    void GattClientCharacteristic::EnableIndication(const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().EnableIndication(valueHandle, onDone);
    }

    void GattClientCharacteristic::DisableIndication(const infra::Function<void(uint8_t)>& onDone)
    {
        GattClientUpdateObserver::Subject().DisableIndication(valueHandle, onDone);
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
