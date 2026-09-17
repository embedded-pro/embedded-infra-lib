#include "services/ble/RetryGattClientCharacteristicsOperations.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace services
{
    void RetryGattClientCharacteristicsOperations::WriteWithoutResponse(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(OperationStatus)>& onDone)
    {
        operationWriteWithoutResponse.emplace(Operation{ handle, data, onDone });
        TryWriteWithoutResponse();
    }

    void RetryGattClientCharacteristicsOperations::TryWriteWithoutResponse()
    {
        GattClientConnectionDecorator::WriteWithoutResponse(operationWriteWithoutResponse->handle, operationWriteWithoutResponse->data, [this](OperationStatus result)
            {
                if (result == OperationStatus::retry)
                    infra::EventDispatcher::Instance().Schedule([this]()
                        {
                            TryWriteWithoutResponse();
                        });
                else
                {
                    operationWriteWithoutResponse->onDone(result);
                    operationWriteWithoutResponse = std::nullopt;
                }
            });
    }
}
