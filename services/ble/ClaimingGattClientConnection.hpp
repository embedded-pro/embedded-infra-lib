#ifndef SERVICES_CLAIMING_GATT_CLIENT_CONNECTION_HPP
#define SERVICES_CLAIMING_GATT_CLIENT_CONNECTION_HPP

#include "infra/event/ClaimableResource.hpp"
#include "services/ble/GattClientConnection.hpp"
#include "services/ble/GattClientLongOperations.hpp"
#include <optional>
#include <variant>

namespace services
{
    class ClaimingGattClientConnection
        : public GattClientConnectionDecorator
        , public GattClientLongOperations
    {
    public:
        using GattClientConnectionDecorator::GattClientConnectionDecorator;

        using GattClientConnectionDecorator::DiscoverCharacteristics;
        using GattClientConnectionDecorator::DiscoverDescriptors;
        using GattClientConnectionDecorator::DiscoverIncludedServices;

        // Implementation of GattClientConnection
        GattRequestStatus ExchangeMtu(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverServices(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverIncludedServices(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;

        // Implementation of GattClientLongOperations
        GattRequestStatus ReadLong(AttAttribute::Handle handle, infra::BoundedVector<uint8_t>& value, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus WriteLong(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) override;

    private:
        using DiscoveryProcedure = infra::Function<GattRequestStatus(const infra::Function<void(GattResult)>&)>;

        GattRequestStatus ClaimDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone, const DiscoveryProcedure& procedure);
        GattRequestStatus ClaimCharacteristicOperation();
        GattRequestStatus PerformCharacteristicOperation();
        void ReportCharacteristicOperationRefused(GattResult result);

        uint16_t MaximumWritePayloadSize() const;
        uint16_t LongReadChunkSize() const;
        uint16_t LongWriteChunkSize() const;

        GattRequestStatus ContinueLongRead();
        void LongReadChunkReceived(GattResult result, infra::ConstByteRange data);
        void CompleteLongRead(GattResult result);

        infra::ConstByteRange CurrentLongWriteChunk() const;
        GattRequestStatus ContinueLongWrite();
        void LongWriteChunkPrepared(GattResult result, uint16_t offset, infra::ConstByteRange echoed);
        void CancelLongWrite(GattResult result);
        void CompleteLongWrite(GattResult result);

    private:
        struct DiscoveryOperation //NOSONAR
        {
            AttAttribute::Handle handle;
            AttAttribute::Handle endHandle;
            infra::Function<void(GattResult)> onDone;
            DiscoveryProcedure procedure;
        };

        struct ReadOperation //NOSONAR
        {
            infra::Function<void(GattResult, infra::ConstByteRange)> onDone;
        };

        struct WriteOperation //NOSONAR
        {
            infra::ConstByteRange data;
            infra::Function<void(GattResult)> onDone;
        };

        struct DescriptorOperation //NOSONAR
        {
            infra::Function<void(GattResult)> onDone;
            DiscoveryProcedure procedure;
        };

        struct LongReadOperation //NOSONAR
        {
            infra::BoundedVector<uint8_t>* value;
            infra::Function<void(GattResult, infra::ConstByteRange)> onDone;
        };

        struct LongWriteOperation //NOSONAR
        {
            infra::ConstByteRange data;
            uint16_t offset;
            GattResult pendingResult;
            infra::Function<void(GattResult)> onDone;
        };

        struct CharacteristicOperation //NOSONAR
        {
            using Operation = std::variant<ReadOperation, WriteOperation, DescriptorOperation, LongReadOperation, LongWriteOperation>;

            CharacteristicOperation(const Operation& operation, AttAttribute::Handle handle)
                : operation(operation)
                , handle(handle)
            {}

            Operation operation;
            AttAttribute::Handle handle;
        };

        std::optional<infra::Function<void(GattResult)>> mtuExchangeContext;
        std::optional<DiscoveryOperation> discoveryContext;
        std::optional<CharacteristicOperation> characteristicOperationContext;

        infra::ClaimableResource resource;
        infra::ClaimableResource::Claimer characteristicOperationsClaimer{ resource };
        infra::ClaimableResource::Claimer discoveryClaimer{ resource };
        infra::ClaimableResource::Claimer attMtuExchangeClaimer{ resource };
    };
}

#endif
