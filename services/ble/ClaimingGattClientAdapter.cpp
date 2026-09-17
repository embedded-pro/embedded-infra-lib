#include "services/ble/ClaimingGattClientAdapter.hpp"

namespace services
{
    ClaimingGattClientAdapter::ClaimingGattClientAdapter(GattClientConnection& connection, GapCentral& gapCentral)
        : GattClientConnectionDecorator(connection)
        , GapCentralObserver(gapCentral)
    {}

    void ClaimingGattClientAdapter::ExchangeMtu()
    {
        attMtuExchangeClaimer.Claim([this]()
            {
                GattClientConnectionDecorator::ExchangeMtu();
            });
    }

    void ClaimingGattClientAdapter::StartServiceDiscovery()
    {
        discoveryClaimer.Claim([this]()
            {
                GattClientConnectionDecorator::StartServiceDiscovery();
            });
    }

    void ClaimingGattClientAdapter::StartCharacteristicDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle)
    {
        discoveryContext.emplace(HandleRange{ handle, endHandle });
        discoveryClaimer.Claim([this]()
            {
                GattClientConnectionDecorator::StartCharacteristicDiscovery(discoveryContext->startHandle, discoveryContext->endHandle);
            });
    }

    void ClaimingGattClientAdapter::StartDescriptorDiscovery(AttAttribute::Handle handle, AttAttribute::Handle endHandle)
    {
        discoveryContext.emplace(HandleRange{ handle, endHandle });
        discoveryClaimer.Claim([this]()
            {
                GattClientConnectionDecorator::StartDescriptorDiscovery(discoveryContext->startHandle, discoveryContext->endHandle);
            });
    }

    void ClaimingGattClientAdapter::ServiceDiscoveryComplete()
    {
        discoveryClaimer.Release();
        GattClientConnectionDecorator::ServiceDiscoveryComplete();
    }

    void ClaimingGattClientAdapter::CharacteristicDiscoveryComplete()
    {
        discoveryClaimer.Release();
        GattClientConnectionDecorator::CharacteristicDiscoveryComplete();
    }

    void ClaimingGattClientAdapter::DescriptorDiscoveryComplete()
    {
        discoveryClaimer.Release();
        GattClientConnectionDecorator::DescriptorDiscoveryComplete();
    }

    void ClaimingGattClientAdapter::MtuChanged()
    {
        GattClientConnectionDecorator::MtuChanged();
        attMtuExchangeClaimer.Release();
    }

    void ClaimingGattClientAdapter::Read(AttAttribute::Handle handle, const infra::Function<void(const infra::ConstByteRange&)>& onRead, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(ReadOperation{ onRead, onDone }, handle);
        characteristicOperationsClaimer.Claim([this]()
            {
                const auto& readContext = std::get<ReadOperation>(characteristicOperationContext->operation);
                GattClientConnectionDecorator::Read(characteristicOperationContext->handle, readContext.onRead, [this](uint8_t result)
                    {
                        characteristicOperationsClaimer.Release();
                        const auto& readContext = std::get<ReadOperation>(characteristicOperationContext->operation);
                        readContext.onDone(result);
                    });
            });
    }

    void ClaimingGattClientAdapter::Write(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(WriteOperation{ data, onDone }, handle);
        characteristicOperationsClaimer.Claim([this]()
            {
                const auto& writeContext = std::get<WriteOperation>(characteristicOperationContext->operation);
                GattClientConnectionDecorator::Write(characteristicOperationContext->handle, writeContext.data, [this](uint8_t result)
                    {
                        characteristicOperationsClaimer.Release();
                        const auto& writeContext = std::get<WriteOperation>(characteristicOperationContext->operation);
                        writeContext.onDone(result);
                    });
            });
    }

    void ClaimingGattClientAdapter::EnableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(uint8_t)>& callback)
                                                   {
                                                       GattClientConnectionDecorator::EnableNotification(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        PerformDescriptorOperation();
    }

    void ClaimingGattClientAdapter::DisableNotification(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(uint8_t)>& callback)
                                                   {
                                                       GattClientConnectionDecorator::DisableNotification(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        PerformDescriptorOperation();
    }

    void ClaimingGattClientAdapter::EnableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(uint8_t)>& callback)
                                                   {
                                                       GattClientConnectionDecorator::EnableIndication(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        PerformDescriptorOperation();
    }

    void ClaimingGattClientAdapter::DisableIndication(AttAttribute::Handle handle, const infra::Function<void(uint8_t)>& onDone)
    {
        characteristicOperationContext.emplace(DescriptorOperation{ onDone, [this](const infra::Function<void(uint8_t)>& callback)
                                                   {
                                                       GattClientConnectionDecorator::DisableIndication(characteristicOperationContext->handle, callback);
                                                   } },
            handle);

        PerformDescriptorOperation();
    }

    void ClaimingGattClientAdapter::PerformDescriptorOperation()
    {
        characteristicOperationsClaimer.Claim([this]()
            {
                auto descriptorOperationContext = std::get<DescriptorOperation>(characteristicOperationContext->operation);
                descriptorOperationContext.operation([this](uint8_t result)
                    {
                        characteristicOperationsClaimer.Release();
                        auto descriptorOperationContext = std::get<DescriptorOperation>(characteristicOperationContext->operation);
                        descriptorOperationContext.onDone(result);
                    });
            });
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
