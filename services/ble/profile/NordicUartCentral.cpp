#include "services/ble/profile/NordicUartCentral.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"

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
    NordicUartCentral::NordicUartCentral(GattClientConnection& connection, WriteMode writeMode)
        : GattClientConnectionObserver(connection)
        , writeMode(writeMode)
    {}

    NordicUartCentral::~NordicUartCentral()
    {
        GattClientCharacteristicUpdateObserver::Detach();
    }

    GattRequestStatus NordicUartCentral::Discover(const infra::Function<void(GattResult)>& onDone)
    {
        really_assert(!onDiscoveryDone);
        really_assert(!open);

        GattClientCharacteristicUpdateObserver::Detach();
        tx = std::nullopt;
        rx = std::nullopt;
        service = std::nullopt;

        auto status = GattClientConnectionObserver::Subject().DiscoverServices([this](GattResult result)
            {
                ServicesDiscovered(result);
            });

        if (status == GattRequestStatus::accepted)
            onDiscoveryDone = onDone;

        return status;
    }

    void NordicUartCentral::Close()
    {
        if (!open)
            return;

        open = false;

        CompleteSend();

        NotifyObservers([](auto& observer)
            {
                observer.Closed();
            });
    }

    bool NordicUartCentral::IsOpen() const
    {
        return open;
    }

    std::size_t NordicUartCentral::MaxSendSize() const
    {
        return GattClientConnectionObserver::Subject().EffectiveMaxAttMtuSize() - attValueHeaderSize;
    }

    void NordicUartCentral::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        really_assert(open);
        really_assert(!onSendCompletion);

        onSendCompletion = actionOnCompletion;
        remaining = data;

        SendNextChunk();
    }

    void NordicUartCentral::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        this->dataReceived = dataReceived;
    }

    void NordicUartCentral::ServiceDiscovered(const GattService& service)
    {
        if (!this->service && service.Type() == AttAttribute::Uuid{ uuid::nordicUartService })
            this->service.emplace(service.Type(), service.Handle(), service.EndHandle());
    }

    void NordicUartCentral::IncludedServiceDiscovered(const GattIncludedService& includedService)
    {}

    void NordicUartCentral::CharacteristicDiscovered(const GattCharacteristic& characteristic)
    {
        auto& connection = GattClientConnectionObserver::Subject();

        if (!rx && characteristic.Type() == AttAttribute::Uuid{ uuid::nordicUartRx })
            rx.emplace(connection, characteristic.Type(), characteristic.Handle(), characteristic.ValueHandle(), characteristic.Properties());
        else if (!tx && characteristic.Type() == AttAttribute::Uuid{ uuid::nordicUartTx })
            tx.emplace(connection, characteristic.Type(), characteristic.Handle(), characteristic.ValueHandle(), characteristic.Properties());
    }

    void NordicUartCentral::DescriptorDiscovered(const GattDescriptor& descriptor)
    {}

    void NordicUartCentral::MtuChanged(uint16_t mtu)
    {}

    void NordicUartCentral::NotificationReceived(infra::ConstByteRange data)
    {
        if (dataReceived)
            dataReceived(data);
    }

    void NordicUartCentral::IndicationReceived(infra::ConstByteRange data, const infra::Function<void()>& onDone)
    {
        onDone();
    }

    void NordicUartCentral::ServicesDiscovered(GattResult result)
    {
        if (result != GattResult::success)
            return CompleteDiscovery(result);

        if (!service)
            return CompleteDiscovery(GattResult::unsupported);

        auto status = GattClientConnectionObserver::Subject().DiscoverCharacteristics(*service, [this](GattResult result)
            {
                CharacteristicsDiscovered(result);
            });

        if (status != GattRequestStatus::accepted)
            CompleteDiscovery(ResultFromRefusedRequest(status));
    }

    void NordicUartCentral::CharacteristicsDiscovered(GattResult result)
    {
        if (result != GattResult::success)
            return CompleteDiscovery(result);

        if (!rx || !tx)
            return CompleteDiscovery(GattResult::unsupported);

        GattClientCharacteristicUpdateObserver::Attach(*tx);

        auto status = tx->EnableNotification([this](GattResult result)
            {
                NotificationEnabled(result);
            });

        if (status != GattRequestStatus::accepted)
            CompleteDiscovery(ResultFromRefusedRequest(status));
    }

    void NordicUartCentral::NotificationEnabled(GattResult result)
    {
        if (result != GattResult::success)
            return CompleteDiscovery(result);

        open = true;

        NotifyObservers([](auto& observer)
            {
                observer.Opened();
            });

        CompleteDiscovery(GattResult::success);
    }

    void NordicUartCentral::CompleteDiscovery(GattResult result)
    {
        if (onDiscoveryDone)
            onDiscoveryDone(result);
    }

    bool NordicUartCentral::WritesWithoutResponse() const
    {
        return writeMode == WriteMode::withoutResponse &&
               (rx->Properties() & GattCharacteristic::PropertyFlags::writeWithoutResponse) == GattCharacteristic::PropertyFlags::writeWithoutResponse;
    }

    void NordicUartCentral::SendNextChunk()
    {
        if (remaining.empty() || !open)
            return CompleteSend();

        auto chunk = infra::Head(remaining, MaxSendSize());

        auto status = WritesWithoutResponse() ? rx->WriteWithoutResponse(chunk) : rx->Write(chunk, [this](GattResult result)
                                                                                      {
                                                                                          ChunkWritten(result);
                                                                                      });

        if (status != GattRequestStatus::accepted)
            return CompleteSend();

        remaining = infra::DiscardHead(remaining, chunk.size());

        if (WritesWithoutResponse())
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    SendNextChunk();
                });
    }

    void NordicUartCentral::ChunkWritten(GattResult result)
    {
        if (result == GattResult::disconnected)
            return Close();

        if (result != GattResult::success)
            return CompleteSend();

        SendNextChunk();
    }

    void NordicUartCentral::CompleteSend()
    {
        remaining = infra::ConstByteRange{};

        if (onSendCompletion)
            onSendCompletion();
    }
}
