#ifndef SERVICES_GATT_CLIENT_CONNECTION_HPP
#define SERVICES_GATT_CLIENT_CONNECTION_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include "services/ble/GattTypes.hpp"

namespace services
{
    class GattClientConnection;

    class GattClientConnectionObserver
        : public infra::Observer<GattClientConnectionObserver, GattClientConnection>
    {
    public:
        using infra::Observer<GattClientConnectionObserver, GattClientConnection>::Observer;

        virtual void ServiceDiscovered(const GattService& service) = 0;
        virtual void IncludedServiceDiscovered(const GattIncludedService& includedService) = 0;
        virtual void CharacteristicDiscovered(const GattCharacteristic& characteristic) = 0;
        virtual void DescriptorDiscovered(const GattDescriptor& descriptor) = 0;
        virtual void MtuChanged(uint16_t mtu) = 0;
    };

    class GattClientUpdateObserver
        : public infra::Observer<GattClientUpdateObserver, GattClientConnection>
    {
    public:
        using infra::Observer<GattClientUpdateObserver, GattClientConnection>::Observer;

        virtual void NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data) = 0;
        virtual void IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone) = 0;
    };

    // An indication is acknowledged once, so whoever hands it to the update observers gives each
    // of them its own completion and reports upwards only after the last one is finished.
    class GattIndicationFanOut
    {
    public:
        void Deliver(infra::Subject<GattClientUpdateObserver>& observers, AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone);

    private:
        void Handled();

    private:
        infra::AutoResetFunction<void()> onDone;
        uint32_t outstanding{ 0 };
    };

    class GattClientConnection
        : public infra::Subject<GattClientConnectionObserver>
        , public infra::Subject<GattClientUpdateObserver>
    {
    public:
        virtual uint16_t EffectiveMaxAttMtuSize() const = 0;

        virtual GattRequestStatus ExchangeMtu(const infra::Function<void(GattResult)>& onDone) = 0;

        virtual GattRequestStatus DiscoverServices(const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus DiscoverIncludedServices(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) = 0;

        GattRequestStatus DiscoverCharacteristics(const GattService& service, const infra::Function<void(GattResult)>& onDone);
        GattRequestStatus DiscoverDescriptors(const GattService& service, const infra::Function<void(GattResult)>& onDone);
        GattRequestStatus DiscoverIncludedServices(const GattService& service, const infra::Function<void(GattResult)>& onDone);

        virtual GattRequestStatus Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) = 0;
        virtual GattRequestStatus Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data) = 0;

        // The single-PDU procedures a long read or long write is composed from. Each is one
        // request and one response, so the status and onDone contract above holds for them
        // unchanged. Composing them is GattClientLongOperations' job, not a caller's.
        virtual GattRequestStatus ReadBlob(AttAttribute::Handle handle, uint16_t offset, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) = 0;

        // onDone receives the offset and value the peer echoed back. The specification requires
        // the client to verify that echo and to cancel the queue when it does not match, so a
        // completion that dropped them would make conformant behaviour impossible.
        virtual GattRequestStatus PrepareWrite(AttAttribute::Handle handle, uint16_t offset, infra::ConstByteRange data, const infra::Function<void(GattResult, uint16_t, infra::ConstByteRange)>& onDone) = 0;
        virtual GattRequestStatus ExecuteWrite(GattExecuteWriteFlag flag, const infra::Function<void(GattResult)>& onDone) = 0;

        virtual GattRequestStatus EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) = 0;
        virtual GattRequestStatus DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) = 0;
    };

    class GattClientConnectionDecorator
        : public GattClientConnectionObserver
        , public GattClientUpdateObserver
        , public GattClientConnection
    {
    public:
        explicit GattClientConnectionDecorator(GattClientConnection& connection);

        using GattClientConnection::DiscoverCharacteristics;
        using GattClientConnection::DiscoverDescriptors;
        using GattClientConnection::DiscoverIncludedServices;

        // Implementation of GattClientConnectionObserver
        void ServiceDiscovered(const GattService& service) override;
        void IncludedServiceDiscovered(const GattIncludedService& includedService) override;
        void CharacteristicDiscovered(const GattCharacteristic& characteristic) override;
        void DescriptorDiscovered(const GattDescriptor& descriptor) override;
        void MtuChanged(uint16_t mtu) override;

        // Implementation of GattClientUpdateObserver
        void NotificationReceived(AttAttribute::Handle handle, infra::ConstByteRange data) override;
        void IndicationReceived(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

        // Implementation of GattClientConnection
        uint16_t EffectiveMaxAttMtuSize() const override;
        GattRequestStatus ExchangeMtu(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverServices(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverIncludedServices(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data) override;
        GattRequestStatus ReadBlob(AttAttribute::Handle handle, uint16_t offset, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus PrepareWrite(AttAttribute::Handle handle, uint16_t offset, infra::ConstByteRange data, const infra::Function<void(GattResult, uint16_t, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus ExecuteWrite(GattExecuteWriteFlag flag, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;

    private:
        GattIndicationFanOut indicationFanOut;
    };
}

#endif
