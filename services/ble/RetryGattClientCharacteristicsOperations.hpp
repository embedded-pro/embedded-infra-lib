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
        GattRequestStatus WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data) override;

    private:
        void TryWriteWithoutResponse();

    private:
        struct Operation
        {
            AttAttribute::Handle handle;
            infra::ConstByteRange data;
        };

        std::optional<Operation> operationWriteWithoutResponse;
    };
}

#endif
