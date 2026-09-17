#ifndef SERVICES_GATT_CLIENT_CHARACTERISTIC_HPP
#define SERVICES_GATT_CLIENT_CHARACTERISTIC_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/IntrusiveForwardList.hpp"
#include "services/ble/GattClientConnection.hpp"

namespace services
{
    class GattClientCharacteristicUpdate;

    class GattClientCharacteristicUpdateObserver
        : public infra::Observer<GattClientCharacteristicUpdateObserver, GattClientCharacteristicUpdate>
    {
    public:
        using infra::Observer<GattClientCharacteristicUpdateObserver, GattClientCharacteristicUpdate>::Observer;

        virtual void NotificationReceived(infra::ConstByteRange data) = 0;
        virtual void IndicationReceived(infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
    };

    class GattClientCharacteristicUpdate
        : public infra::Subject<GattClientCharacteristicUpdateObserver>
    {};

    class GattClientCharacteristic
        : public infra::IntrusiveForwardList<GattClientCharacteristic>::NodeType
        , public GattCharacteristic
        , public GattClientCharacteristicUpdate
        , protected GattClientUpdateObserver
    {
    public:
        GattClientCharacteristic(GattClientConnection& connection, AttAttribute::Uuid type, AttAttribute::Handle handle, AttAttribute::Handle valueHandle, GattCharacteristic::PropertyFlags properties);

        virtual GattRequestStatus Read(const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone);
        virtual GattRequestStatus Write(infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone);
        virtual GattRequestStatus WriteWithoutResponse(infra::ConstByteRange data);

        virtual GattRequestStatus EnableNotification(const infra::Function<void(GattResult)>& onDone);
        virtual GattRequestStatus DisableNotification(const infra::Function<void(GattResult)>& onDone);
        virtual GattRequestStatus EnableIndication(const infra::Function<void(GattResult)>& onDone);
        virtual GattRequestStatus DisableIndication(const infra::Function<void(GattResult)>& onDone);

        GattCharacteristic::PropertyFlags CharacteristicProperties() const;
        AttAttribute::Handle CharacteristicValueHandle() const;

        // Implementation of GattClientUpdateObserver
        void NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data) override;
        void IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

    private:
        infra::AutoResetFunction<void()> onIndicationDone;
        uint32_t observers{ 0 };
    };

    class GattClientService
        : public GattService
        , public infra::IntrusiveForwardList<GattClientService>::NodeType
    {
    public:
        explicit GattClientService(const AttAttribute::Uuid& type, const AttAttribute::Handle& handle, const AttAttribute::Handle& endHandle);

        void AddCharacteristic(GattClientCharacteristic& characteristic);
        const infra::IntrusiveForwardList<GattClientCharacteristic>& Characteristics() const;

    private:
        infra::IntrusiveForwardList<GattClientCharacteristic> characteristics;
    };
}

#endif
