#include "services/ble/profile/NordicUartPeripheral.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace services
{
    NordicUartPeripheral::NordicUartPeripheral(GattServer& gattServer, uint16_t maxAttMtuSize)
        : maxPayloadSize(static_cast<uint16_t>(maxAttMtuSize - attValueHeaderSize))
        , payloadSize(static_cast<uint16_t>(attDefaultMaxMtuSize - attValueHeaderSize))
        , rx(service, uuid::nordicUartRx, maxPayloadSize, GattCharacteristic::PropertyFlags::write | GattCharacteristic::PropertyFlags::writeWithoutResponse)
        , tx(service, uuid::nordicUartTx, maxPayloadSize, GattCharacteristic::PropertyFlags::notify)
    {
        really_assert(maxAttMtuSize >= attDefaultMaxMtuSize);

        GattServerCharacteristicObserver::Attach(rx);
        gattServer.AddService(service);
    }

    NordicUartPeripheral::~NordicUartPeripheral()
    {
        GattServerCharacteristicObserver::Detach();
    }

    void NordicUartPeripheral::MaxAttMtuSizeChanged(uint16_t mtu)
    {
        really_assert(mtu >= attDefaultMaxMtuSize);

        payloadSize = std::min(static_cast<uint16_t>(mtu - attValueHeaderSize), maxPayloadSize);
    }

    void NordicUartPeripheral::NotificationsEnabled(bool enabled)
    {
        if (enabled == open)
            return;

        open = enabled;

        if (open)
            NotifyObservers([](auto& observer)
                {
                    observer.Opened();
                });
        else
        {
            CompleteSend();

            NotifyObservers([](auto& observer)
                {
                    observer.Closed();
                });
        }
    }

    GattServerService& NordicUartPeripheral::Service()
    {
        return service;
    }

    bool NordicUartPeripheral::IsOpen() const
    {
        return open;
    }

    std::size_t NordicUartPeripheral::MaxSendSize() const
    {
        return payloadSize;
    }

    void NordicUartPeripheral::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        really_assert(open);
        really_assert(!onSendCompletion);

        onSendCompletion = actionOnCompletion;
        remaining = data;

        SendNextChunk();
    }

    void NordicUartPeripheral::ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived)
    {
        this->dataReceived = dataReceived;
    }

    void NordicUartPeripheral::DataReceived(infra::ConstByteRange data)
    {
        if (dataReceived)
            dataReceived(data);
    }

    void NordicUartPeripheral::SendNextChunk()
    {
        if (remaining.empty() || !open)
            return CompleteSend();

        auto chunk = infra::Head(remaining, MaxSendSize());
        remaining = infra::DiscardHead(remaining, chunk.size());

        tx.Update(chunk, [this]()
            {
                infra::EventDispatcher::Instance().Schedule([this]()
                    {
                        SendNextChunk();
                    });
            });
    }

    void NordicUartPeripheral::CompleteSend()
    {
        remaining = infra::ConstByteRange{};

        if (onSendCompletion)
            onSendCompletion();
    }
}
