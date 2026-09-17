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

    GattRequestStatus ClaimingGattClientConnection::DiscoverIncludedServices(AttAttribute::Handle handle, AttAttribute::Handle endHandle, const infra::Function<void(GattResult)>& onDone)
    {
        return ClaimDiscovery(handle, endHandle, onDone, [this](const infra::Function<void(GattResult)>& callback)
            {
                return GattClientConnectionDecorator::DiscoverIncludedServices(discoveryContext->handle, discoveryContext->endHandle, callback);
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

    GattRequestStatus ClaimingGattClientConnection::ReadLong(AttAttribute::Handle handle, infra::BoundedVector<uint8_t>& value, const infra::Function<void(GattResult, infra::ConstByteRange)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        value.clear();

        characteristicOperationContext.emplace(LongReadOperation{ &value, onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    GattRequestStatus ClaimingGattClientConnection::WriteLong(AttAttribute::Handle handle, infra::ConstByteRange data, const infra::Function<void(GattResult)>& onDone)
    {
        if (characteristicOperationsClaimer.IsClaimed() || characteristicOperationsClaimer.IsQueued())
            return GattRequestStatus::busy;

        // The offset carried in a Prepare Write Request is sixteen bits.
        if (data.size() > attMaxAttributeValueSize)
            return GattRequestStatus::invalidParameter;

        characteristicOperationContext.emplace(LongWriteOperation{ data, 0, GattResult::success, onDone }, handle);

        return ClaimCharacteristicOperation();
    }

    uint16_t ClaimingGattClientConnection::MaximumWritePayloadSize() const
    {
        // Opcode and handle occupy three octets of a Write Request.
        return static_cast<uint16_t>(EffectiveMaxAttMtuSize() - 3);
    }

    uint16_t ClaimingGattClientConnection::LongReadChunkSize() const
    {
        // The opcode occupies one octet of a Read Blob Response.
        return static_cast<uint16_t>(EffectiveMaxAttMtuSize() - 1);
    }

    uint16_t ClaimingGattClientConnection::LongWriteChunkSize() const
    {
        // Opcode, handle and offset occupy five octets of a Prepare Write Request.
        return static_cast<uint16_t>(EffectiveMaxAttMtuSize() - 5);
    }

    GattRequestStatus ClaimingGattClientConnection::ContinueLongRead()
    {
        auto& operation = std::get<LongReadOperation>(characteristicOperationContext->operation);
        auto offset = static_cast<uint16_t>(operation.value->size());

        auto chunkReceived = [this](GattResult result, infra::ConstByteRange data)
        {
            LongReadChunkReceived(result, data);
        };

        if (offset == 0)
            return GattClientConnectionDecorator::Read(characteristicOperationContext->handle, chunkReceived);

        return GattClientConnectionDecorator::ReadBlob(characteristicOperationContext->handle, offset, chunkReceived);
    }

    void ClaimingGattClientConnection::LongReadChunkReceived(GattResult result, infra::ConstByteRange data)
    {
        auto& operation = std::get<LongReadOperation>(characteristicOperationContext->operation);

        // A Read Blob Request past the end of the value is answered with Invalid Offset, which is
        // the end of the value rather than a failure. attributeNotLong is a peer refusing Read Blob
        // outright and must not be read as one.
        if (result == GattResult::invalidOffset && !operation.value->empty())
            return CompleteLongRead(GattResult::success);

        if (result != GattResult::success)
            return CompleteLongRead(result);

        if (operation.value->max_size() - operation.value->size() < data.size())
        {
            operation.value->insert(operation.value->end(), data.begin(), data.begin() + (operation.value->max_size() - operation.value->size()));
            return CompleteLongRead(GattResult::insufficientResources);
        }

        operation.value->insert(operation.value->end(), data.begin(), data.end());

        if (data.size() < LongReadChunkSize())
            return CompleteLongRead(GattResult::success);

        auto status = ContinueLongRead();

        if (status != GattRequestStatus::accepted)
            CompleteLongRead(ResultFromRefusedRequest(status));
    }

    void ClaimingGattClientConnection::CompleteLongRead(GattResult result)
    {
        auto& operation = std::get<LongReadOperation>(characteristicOperationContext->operation);
        auto value = infra::MakeRange(*operation.value);

        characteristicOperationsClaimer.Release();
        operation.onDone(result, value);
    }

    infra::ConstByteRange ClaimingGattClientConnection::CurrentLongWriteChunk() const
    {
        const auto& operation = std::get<LongWriteOperation>(characteristicOperationContext->operation);
        auto remaining = operation.data.size() - operation.offset;

        return infra::ConstByteRange(operation.data.begin() + operation.offset, operation.data.begin() + operation.offset + std::min<std::size_t>(remaining, LongWriteChunkSize()));
    }

    GattRequestStatus ClaimingGattClientConnection::ContinueLongWrite()
    {
        const auto& operation = std::get<LongWriteOperation>(characteristicOperationContext->operation);

        return GattClientConnectionDecorator::PrepareWrite(characteristicOperationContext->handle, operation.offset, CurrentLongWriteChunk(), [this](GattResult result, uint16_t offset, infra::ConstByteRange echoed)
            {
                LongWriteChunkPrepared(result, offset, echoed);
            });
    }

    void ClaimingGattClientConnection::LongWriteChunkPrepared(GattResult result, uint16_t offset, infra::ConstByteRange echoed)
    {
        auto& operation = std::get<LongWriteOperation>(characteristicOperationContext->operation);

        if (result != GattResult::success)
            return CancelLongWrite(result);

        auto expected = CurrentLongWriteChunk();

        // The specification requires the client to verify the echo and to cancel the queue when it
        // does not match.
        if (offset != operation.offset || !infra::ContentsEqual(expected, echoed))
            return CancelLongWrite(GattResult::unknown);

        operation.offset = static_cast<uint16_t>(operation.offset + expected.size());

        auto status = operation.offset == operation.data.size()
                          ? GattClientConnectionDecorator::ExecuteWrite(GattExecuteWriteFlag::write, [this](GattResult executeResult)
                                {
                                    CompleteLongWrite(executeResult);
                                })
                          : ContinueLongWrite();

        if (status != GattRequestStatus::accepted)
            CancelLongWrite(ResultFromRefusedRequest(status));
    }

    void ClaimingGattClientConnection::CancelLongWrite(GattResult result)
    {
        auto& operation = std::get<LongWriteOperation>(characteristicOperationContext->operation);
        operation.pendingResult = result;

        auto status = GattClientConnectionDecorator::ExecuteWrite(GattExecuteWriteFlag::cancel, [this](GattResult)
            {
                CompleteLongWrite(std::get<LongWriteOperation>(characteristicOperationContext->operation).pendingResult);
            });

        if (status != GattRequestStatus::accepted)
            CompleteLongWrite(result);
    }

    void ClaimingGattClientConnection::CompleteLongWrite(GattResult result)
    {
        auto& operation = std::get<LongWriteOperation>(characteristicOperationContext->operation);

        characteristicOperationsClaimer.Release();
        operation.onDone(result);
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
                              },
                              [this](const LongReadOperation&)
                              {
                                  return ContinueLongRead();
                              },
                              [this](const LongWriteOperation& longWrite)
                              {
                                  if (longWrite.data.size() <= MaximumWritePayloadSize())
                                      return GattClientConnectionDecorator::Write(characteristicOperationContext->handle, longWrite.data, [this](GattResult result)
                                          {
                                              CompleteLongWrite(result);
                                          });

                                  return ContinueLongWrite();
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
                       },
                       [result](const LongReadOperation& longRead)
                       {
                           longRead.onDone(result, infra::ConstByteRange());
                       },
                       [result](const LongWriteOperation& longWrite)
                       {
                           longWrite.onDone(result);
                       } },
            characteristicOperationContext->operation);
    }
}
