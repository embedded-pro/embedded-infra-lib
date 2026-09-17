#ifndef SERVICES_CLAIMING_GATT_CLIENT_CONNECTION_HPP
#define SERVICES_CLAIMING_GATT_CLIENT_CONNECTION_HPP

#include "infra/event/ClaimableResource.hpp"
#include "services/ble/GattClientConnection.hpp"
#include <optional>
#include <variant>

namespace services
{
    class ClaimingGattClientConnection
        : public GattClientConnectionDecorator
    {
    public:
        using GattClientConnectionDecorator::GattClientConnectionDecorator;

        // Implementation of GattClientConnection
        GattRequestStatus ExchangeMtu(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverServices(const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone) override;
        GattRequestStatus Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;
        GattRequestStatus DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone) override;

    private:
        using DiscoveryProcedure = infra::Function<GattRequestStatus(const infra::Function<void(GattResult)>&)>;

        GattRequestStatus ClaimDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone, const DiscoveryProcedure& procedure);
        GattRequestStatus ClaimCharacteristicOperation();
        GattRequestStatus PerformCharacteristicOperation();
        void ReportCharacteristicOperationRefused(GattResult result);

    private:
        struct DiscoveryOperation
        {
            AttAttribute::Handle handle;
            AttAttribute::Handle endHandle;
            const infra::Function<void(GattResult)> onDone;
            DiscoveryProcedure procedure;
        };

        struct ReadOperation
        {
            const infra::Function<void(GattResult, infra::ConstByteRange)> onDone;
        };

        struct WriteOperation
        {
            infra::ConstByteRange data;
            const infra::Function<void(GattResult)> onDone;
        };

        struct DescriptorOperation
        {
            const infra::Function<void(GattResult)> onDone;
            DiscoveryProcedure procedure;
        };

        struct CharacteristicOperation
        {
            using Operation = std::variant<ReadOperation, WriteOperation, DescriptorOperation>;

            CharacteristicOperation(Operation operation, AttAttribute::Handle handle)
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
