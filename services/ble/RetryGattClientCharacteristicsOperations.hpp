#ifndef SERVICES_RETRY_GATT_CLIENT_CHARACTERISTICS_OPERATIONS_HPP
#define SERVICES_RETRY_GATT_CLIENT_CHARACTERISTICS_OPERATIONS_HPP

#include "services/ble/GattClientConnection.hpp"
#include <optional>

namespace services
{
    class RetryGattClientCharacteristicsOperations
        : public GattClientConnectionDecorator
    {
    public:
        using GattClientConnectionDecorator::GattClientConnectionDecorator;

        // Implementation of GattClientConnection
        void WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone) override;

    private:
        void TryWriteWithoutResponse();

    private:
        struct Operation
        {
            AttAttribute::Handle handle;
            infra::ConstByteRange data;
            const infra::Function<void(OperationStatus)> onDone;
        };

        std::optional<Operation> operationWriteWithoutResponse;
    };
}

#endif
