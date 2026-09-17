#ifndef SERVICES_RETRYING_GATT_CLIENT_CONNECTION_HPP
#define SERVICES_RETRYING_GATT_CLIENT_CONNECTION_HPP

#include "infra/timer/Timer.hpp"
#include "services/ble/GattClientConnection.hpp"
#include <optional>

namespace services
{
    class RetryingGattClientConnection
        : public GattClientConnectionDecorator
    {
    public:
        static constexpr infra::Duration defaultRetryInterval = std::chrono::milliseconds(10);

        explicit RetryingGattClientConnection(GattClientConnection& connection, infra::Duration retryInterval = defaultRetryInterval);

        // Implementation of GattClientConnection
        GattRequestStatus WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data) override;

    private:
        void Retry();

    private:
        struct Operation
        {
            AttAttribute::Handle handle;
            infra::ConstByteRange data;
        };

        infra::Duration retryInterval;
        std::optional<Operation> operationWriteWithoutResponse;
        infra::TimerSingleShot retryTimer;
    };
}

#endif
