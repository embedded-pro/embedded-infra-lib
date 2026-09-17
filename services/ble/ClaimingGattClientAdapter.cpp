#include "services/ble/ClaimingGattClientAdapter.hpp"

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
}

namespace services
{
    ClaimingGattClientAdapter::ClaimingGattClientAdapter(GattClientConnection& connection, GapCentral& gapCentral)
        : GattClientConnectionDecorator(connection)
        , GapCentralObserver(gapCentral)
    {}

    GattRequestStatus ClaimingGattClientAdapter::ExchangeMtu(const infra::Function<void(GattResult)>& onDone)
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

    GattRequestStatus ClaimingGattClientAdapter::DiscoverServices(const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(0, 0, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverServices(callback);
            });
    }

    GattRequestStatus ClaimingGattClientAdapter::DiscoverCharacteristics(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(handle, endHandle, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverCharacteristics(discoveryContext->handle, discoveryContext->endHandle, callback);
            });
    }

    GattRequestStatus ClaimingGattClientAdapter::DiscoverDescriptors(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(handle, endHandle, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverDescriptors(discoveryContext->handle, discoveryContext->endHandle, callback);
            });
    }

    GattRequestStatus ClaimingGattClientAdapter::ClaimDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone, const DiscoveryProcedure& procedure)
    {
        if (discoveryClaimer.IsClaimed() || discoveryClaimer.IsQueued())
            return GattRequestStatus::busy;

        discoveryContext.emplace(DiscoveryOperation{ handle, endHandle, onDone, procedure });

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

    GattRequestStatus ClaimingGattClientAdapter::Read(AttAttribute::Handle handle, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(ReadOperation{ onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientAdapter::Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        characteristicOperationContext.emplace(WriteOperation{ data, onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientAdapter::EnableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
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

    GattRequestStatus ClaimingGattClientAdapter::DisableNotification(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
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

    GattRequestStatus ClaimingGattClientAdapter::EnableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
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

    GattRequestStatus ClaimingGattClientAdapter::DisableIndication(AttAttribute::Handle handle, const infra::Function<void(GattResult)>& onDone)
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

    GattRequestStatus ClaimingGattClientAdapter::ClaimCharacteristicOperation()
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

    GattRequestStatus ClaimingGattClientAdapter::PerformCharacteristicOperation()
    {
        auto& context = *characteristicOperationContext;

        if (std::holds_alternative<ReadOperation>(context.operation))
            return GattClientConnectionDecorator::Read(context.handle, [this](GattResult result, infra::ConstByteRange data)
                {
                    characteristicOperationsClaimer.Release();
                    std::get<ReadOperation>(characteristicOperationContext->operation).onDone(result, data);
                });

        if (std::holds_alternative<WriteOperation>(context.operation))
            return GattClientConnectionDecorator::Write(context.handle, std::get<WriteOperation>(context.operation).data, [this](GattResult result)
                {
                    characteristicOperationsClaimer.Release();
                    std::get<WriteOperation>(characteristicOperationContext->operation).onDone(result);
                });

        return std::get<DescriptorOperation>(context.operation).procedure([this](GattResult result)
            {
                characteristicOperationsClaimer.Release();
                std::get<DescriptorOperation>(characteristicOperationContext->operation).onDone(result);
            });
    }

    void ClaimingGattClientAdapter::ReportCharacteristicOperationRefused(GattResult result)
    {
        auto& context = *characteristicOperationContext;

        if (std::holds_alternative<ReadOperation>(context.operation))
            std::get<ReadOperation>(context.operation).onDone(result, infra::ConstByteRange());
        else if (std::holds_alternative<WriteOperation>(context.operation))
            std::get<WriteOperation>(context.operation).onDone(result);
        else
            std::get<DescriptorOperation>(context.operation).onDone(result);
    }

    void ClaimingGattClientAdapter::DeviceDiscovered(const GapAdvertisingReport& deviceDiscovered)
    {}

    void ClaimingGattClientAdapter::StateChanged(GapCentralState state)
    {
        if (state == GapCentralState::standby)
        {
            discoveryClaimer.Release();
            characteristicOperationsClaimer.Release();
            attMtuExchangeClaimer.Release();
        }
    }
}
