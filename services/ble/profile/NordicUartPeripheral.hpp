#ifndef SERVICES_NORDIC_UART_PERIPHERAL_HPP
#define SERVICES_NORDIC_UART_PERIPHERAL_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "services/ble/GattServerCharacteristicImpl.hpp"
#include "services/ble/profile/NordicUart.hpp"

namespace services
{
    class NordicUartPeripheral
        : public NordicUart
        , private GattServerCharacteristicObserver
    {
    public:
        explicit NordicUartPeripheral(GattServer& gattServer, uint16_t maxAttMtuSize = attDefaultMaxMtuSize);
        ~NordicUartPeripheral();

        void MaxAttMtuSizeChanged(uint16_t mtu);
        void NotificationsEnabled(bool enabled);

        GattServerService& Service();

        bool IsOpen() const override;
        std::size_t MaxSendSize() const override;

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
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
