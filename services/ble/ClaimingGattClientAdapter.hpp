#ifndef SERVICES_CLAIMING_GATT_CLIENT_ADAPTER_HPP
#define SERVICES_CLAIMING_GATT_CLIENT_ADAPTER_HPP

#include "infra/event/ClaimableResource.hpp"
#include "services/ble/GapCentral.hpp"
#include "services/ble/GattClientConnection.hpp"
#include <functional>
#include <optional>
#include <variant>

namespace services
{
    class ClaimingGattClientAdapter
        : public GattClientConnectionDecorator
        , private GapCentralObserver
    {
    public:
        ClaimingGattClientAdapter(GattClientConnection& connection, GapCentral& gapCentral);

        // Implementation of GattClientConnection
        void ExchangeMtu() override;
        void StartServiceDiscovery() override;
        void StartCharacteristicDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) override;
        void StartDescriptorDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle) override;
        void Read(AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone) override;
        void Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone) override;
        void EnableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void DisableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void EnableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;
        void DisableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone) override;

    private:
        // Implementation of GattClientConnectionObserver
        void ServiceDiscoveryComplete() override;
        void CharacteristicDiscoveryComplete() override;
        void DescriptorDiscoveryComplete() override;
        void MtuChanged() override;

        // Implementation of GapCentralObserver
        void DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered) override;
        void StateChanged(GapCentralState state) override;

        void PerformDescriptorOperation();

    private:
        struct HandleRange
        {
            AttAttribute::Handle startHandle;
            AttAttribute::Handle endHandle;
        };

        struct ReadOperation
        {
            const infra::Function<void(const infra::ConstByteRange&)> onRead;
            const infra::Function<void(uint8_t)> onDone;
        };

        struct WriteOperation
        {
            infra::ConstByteRange data;
            const infra::Function<void(uint8_t)> onDone;
        };

        struct DescriptorOperation
        {
            const infra::Function<void(uint8_t)> onDone;
            infra::Function<void(const infra::Function<void(uint8_t)>&)> operation;
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

        std::optional<HandleRange> discoveryContext;
        std::optional<CharacteristicOperation> characteristicOperationContext;

        infra::ClaimableResource resource;
        infra::ClaimableResource::Claimer characteristicOperationsClaimer{ resource };
        infra::ClaimableResource::Claimer discoveryClaimer{ resource };
        infra::ClaimableResource::Claimer attMtuExchangeClaimer{ resource };
    };
}

#endif
