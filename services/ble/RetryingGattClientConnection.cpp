#include "services/ble/RetryingGattClientConnection.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    GattRequestStatus RetryingGattClientConnection::WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data)
    {
        if (operationWriteWithoutResponse)
            return GattRequestStatus::busy;

        operationWriteWithoutResponse.emplace(Operation{ handle, data });
        TryWriteWithoutResponse();

        return GattRequestStatus::accepted;
    }

    void RetryingGattClientConnection::TryWriteWithoutResponse()
    {
        auto status = GattClientConnectionDecorator::WriteWithoutResponse(operationWriteWithoutResponse->handle, operationWriteWithoutResponse->data);

        if (status == GattRequestStatus::busy)
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    TryWriteWithoutResponse();
                });
        else
            operationWriteWithoutResponse = std::nullopt;
    }
}
