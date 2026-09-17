#include "services/ble/RetryingGattClientConnection.hpp"

namespace services
{
    RetryingGattClientConnection::RetryingGattClientConnection(GattClientConnection& connection, infra::Duration retryInterval)
        : GattClientConnectionDecorator(connection)
        , retryInterval(retryInterval)
    {}

    GattRequestStatus RetryingGattClientConnection::WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        if (operationWriteWithoutResponse)
            return GattRequestStatus::busy;

        auto status = GattClientConnectionDecorator::WriteWithoutResponse(handle, data);

        if (status != GattRequestStatus::busy)
            return status;

        operationWriteWithoutResponse.emplace(handle, data);
        retryTimer.Start(retryInterval, [this]()
            {
                Retry();
            });

        return GattRequestStatus::accepted;
    }

    void RetryingGattClientConnection::Retry()
    {
        auto status = GattClientConnectionDecorator::WriteWithoutResponse(operationWriteWithoutResponse->handle, operationWriteWithoutResponse->data);

        if (status == GattRequestStatus::busy)
            retryTimer.Start(retryInterval, [this]()
                {
                    Retry();
                });
        else
            operationWriteWithoutResponse = std::nullopt;
    }
}
