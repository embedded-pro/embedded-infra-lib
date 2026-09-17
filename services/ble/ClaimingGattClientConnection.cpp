#include "services/ble/ClaimingGattClientConnection.hpp"

namespace
{
    services::GattResult ResultFromRefusedRequest(services::GattRequestStatus status)
    {
        switch (status)
        {
            case services::GattRequestStatus::invalidState:
                return services::GattResult::disconnected;
            case services::GattRequestStatus::notSupported:
                return services::GattResult::unsupported;
            default:
                return services::GattResult::unknown;
        }
    }

    template<class... Ts>
    struct Overloaded : Ts...
    {
        using Ts::operator()...;
    };

    template<class... Ts>
    Overloaded(Ts...) -> Overloaded<Ts...>;
}

namespace services
{
    GattRequestStatus ClaimingGattClientConnection::ExchangeMtu(const infra::Function<void(GattResult)>& onDone)
    {
        if (attMtuExchangeClaimer.IsClaimed() || attMtuExchangeClaimer.IsQueued())
            return GattRequestStatus::busy;

        mtuExchangeContext.emplace(onDone);

        attMtuExchangeClaimer.Claim([this]()
            {
                auto status = GattClientConnectionDecorator::ExchangeMtu([this](GattResult result)
                    {
                        attMtuExchangeClaimer.Release();
                        (*mtuExchangeContext)(result);
                    });

                if (status != GattRequestStatus::accepted)
                {
                    attMtuExchangeClaimer.Release();
                    (*mtuExchangeContext)(ResultFromRefusedRequest(status));
                }
            });

        return GattRequestStatus::accepted;
    }

    GattRequestStatus ClaimingGattClientConnection::DiscoverServices(const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(0, 0, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverServices(callback);
            });
    }

    GattRequestStatus ClaimingGattClientConnection::DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(handle, endHandle, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverCharacteristics(discoveryContext->handle, discoveryContext->endHandle, callback);
            });
    }

    GattRequestStatus ClaimingGattClientConnection::DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(handle, endHandle, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverDescriptors(discoveryContext->handle, discoveryContext->endHandle, callback);
            });
    }

    GattRequestStatus ClaimingGattClientConnection::ClaimDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone, const DiscoveryProcedure& procedure)
    {
        if (discoveryClaimer.IsClaimed() || discoveryClaimer.IsQueued())
            return GattRequestStatus::busy;

        discoveryContext.emplace(handle, endHandle, onDone, procedure);

        discoveryClaimer.Claim([this]()
            {
                auto status = discoveryContext->procedure([this](GattResult result)
                    {
                        discoveryClaimer.Release();
                        discoveryContext->onDone(result);
                    });

                if (status != GattRequestStatus::accepted)
                {
                    discoveryClaimer.Release();
                    discoveryContext->onDone(ResultFromRefusedRequest(status));
                }
            });

        return GattRequestStatus::accepted;
    }

    GattRequestStatus ClaimingGattClientConnection::Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(ReadOperation{ onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(WriteOperation{ data, onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(GattResult)>& callback)
                                                   {
                                                       return GattClientConnectionDecorator::EnableNotification(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(GattResult)>& callback)
                                                   {
                                                       return GattClientConnectionDecorator::DisableNotification(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(GattResult)>& callback)
                                                   {
                                                       return GattClientConnectionDecorator::EnableIndication(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(GattResult)>& callback)
                                                   {
                                                       return GattClientConnectionDecorator::DisableIndication(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::ClaimCharacteristicOperation()
    {
        characteristicOperationsClaimer.Claim([this]()
            {
                auto status = PerformCharacteristicOperation();

                if (status != GattRequestStatus::accepted)
                {
                    characteristicOperationsClaimer.Release();
                    ReportCharacteristicOperationRefused(ResultFromRefusedRequest(status));
                }
            });

        return GattRequestStatus::accepted;
    }

    GattRequestStatus ClaimingGattClientConnection::PerformCharacteristicOperation()
    {
        return std::visit(Overloaded{ [this](const ReadOperation&)
                              {
                                  return GattClientConnectionDecorator::Read(characteristicOperationContext->handle, [this](GattResult result, infra::ConstByteRange data)
                                      {
                                          characteristicOperationsClaimer.Release();
                                          std::get<ReadOperation>(characteristicOperationContext->operation).onDone(result, data);
                                      });
                              },
                              [this](const WriteOperation& write)
                              {
                                  return GattClientConnectionDecorator::Write(characteristicOperationContext->handle, write.data, [this](GattResult result)
                                      {
                                          characteristicOperationsClaimer.Release();
                                          std::get<WriteOperation>(characteristicOperationContext->operation).onDone(result);
                                      });
                              },
                              [this](const DescriptorOperation& descriptor)
                              {
                                  return descriptor.procedure([this](GattResult result)
                                      {
                                          characteristicOperationsClaimer.Release();
                                          std::get<DescriptorOperation>(characteristicOperationContext->operation).onDone(result);
                                      });
                              } },
            characteristicOperationContext->operation);
    }

    void ClaimingGattClientConnection::ReportCharacteristicOperationRefused(GattResult result)
    {
        std::visit(Overloaded{ [result](const ReadOperation& read)
                       {
                           read.onDone(result, infra::ConstByteRange());
                       },
                       [result](const WriteOperation& write)
                       {
                           write.onDone(result);
                       },
                       [result](const DescriptorOperation& descriptor)
                       {
                           descriptor.onDone(result);
                       } },
            characteristicOperationContext->operation);
    }
}
