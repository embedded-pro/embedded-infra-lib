#ifndef SERVICES_NORDIC_UART_PERIPHERAL_HPP
#define SERVICES_NORDIC_UART_PERIPHERAL_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "services/ble/GattServerCharacteristicImpl.hpp"
#include "services/ble/profile/NordicUart.hpp"

namespace services
{
    // The GATT server half of the Nordic UART Service: it receives on Rx, which the peer writes,
    // and sends on Tx, which the peer subscribes to.
    class NordicUartPeripheral
        : public NordicUart
        , private GattServerCharacteristicObserver
    {
    public:
        // 'maxAttMtuSize' is the largest ATT_MTU the stack is configured to negotiate. It fixes
        // the declared length of both characteristic values and caps what a later negotiation can
        // raise the payload to.
        explicit NordicUartPeripheral(GattServer& gattServer, uint16_t maxAttMtuSize = attDefaultMaxMtuSize);
        ~NordicUartPeripheral();

        // GattServer reports neither the negotiated MTU nor a write to the Client Characteristic
        // Configuration descriptor, so a port drives both from its own stack callbacks. A link
        // that is lost leaves notifications disabled, which a port reports the same way.
        void MaxAttMtuSizeChanged(uint16_t mtu);
        void NotificationsEnabled(bool enabled);

        GattServerService& Service();

        // Implementation of NordicUart
        bool IsOpen() const override;
        std::size_t MaxSendSize() const override;

        // Implementation of hal::SerialCommunication
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        // Implementation of GattServerCharacteristicObserver
        void DataReceived(infra::ConstByteRange data) override;

        void SendNextChunk();
        void CompleteSend();

    private:
        uint16_t maxPayloadSize;
        uint16_t payloadSize;

        GattServerService service{ uuid::nordicUartService };
        GattServerCharacteristicImpl rx;
        GattServerCharacteristicImpl tx;

        bool open{ false };
        infra::ConstByteRange remaining;
        infra::AutoResetFunction<void()> onSendCompletion;
        infra::Function<void(infra::ConstByteRange data)> dataReceived;
    };
}

#endif
