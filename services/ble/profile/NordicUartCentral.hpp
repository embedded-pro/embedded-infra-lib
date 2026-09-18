#ifndef SERVICES_NORDIC_UART_CENTRAL_HPP
#define SERVICES_NORDIC_UART_CENTRAL_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "services/ble/GattClientCharacteristic.hpp"
#include "services/ble/profile/NordicUart.hpp"
#include <optional>

namespace services
{
    // The GATT client half of the Nordic UART Service: it sends by writing Rx and receives the
    // notifications of Tx, both named for the peripheral that owns them.
    class NordicUartCentral
        : public NordicUart
        , private GattClientConnectionObserver
        , private GattClientCharacteristicUpdateObserver
    {
    public:
        enum class WriteMode : uint8_t
        {
            // A Write Request is answered, so completion is reported when the peer has the data.
            withResponse,

            // A Write Command is not answered and can be refused with busy, which only
            // RetryingGattClientConnection re-attempts. Faster, and completion means no more than
            // that the stack took the last chunk. Falls back to withResponse when the peer's Rx
            // characteristic does not offer the property.
            withoutResponse
        };

        explicit NordicUartCentral(GattClientConnection& connection, WriteMode writeMode = WriteMode::withResponse);
        ~NordicUartCentral();

        // Finds the service and its two characteristics and subscribes to Tx. Reports success once
        // the pipe is open. A peer exposing neither the service nor both characteristics reports
        // GattResult::unsupported, which is this profile's reading of that outcome rather than an
        // ATT error code.
        GattRequestStatus Discover(const infra::Function<void(GattResult)>& onDone);

        // Gives up the pipe without writing the Client Characteristic Configuration descriptor: a
        // port calls this when the link is gone. A send in progress reports completion.
        void Close();

        // Implementation of NordicUart
        bool IsOpen() const override;
        std::size_t MaxSendSize() const override;

        // Implementation of hal::SerialCommunication
        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        // Implementation of GattClientConnectionObserver
        void ServiceDiscovered(const GattService& service) override;
        void IncludedServiceDiscovered(const GattIncludedService& includedService) override;
        void CharacteristicDiscovered(const GattCharacteristic& characteristic) override;
        void DescriptorDiscovered(const GattDescriptor& descriptor) override;
        void MtuChanged(uint16_t mtu) override;

        // Implementation of GattClientCharacteristicUpdateObserver
        void NotificationReceived(infra::ConstByteRange data) override;
        void IndicationReceived(infra::ConstByteRange data, const infra::Function<void()>& onDone) override;

        void ServicesDiscovered(GattResult result);
        void CharacteristicsDiscovered(GattResult result);
        void NotificationEnabled(GattResult result);
        void CompleteDiscovery(GattResult result);

        bool WritesWithoutResponse() const;
        void SendNextChunk();
        void ChunkWritten(GattResult result);
        void CompleteSend();

    private:
        WriteMode writeMode;

        std::optional<GattClientService> service;
        std::optional<GattClientCharacteristic> rx;
        std::optional<GattClientCharacteristic> tx;

        bool open{ false };
        infra::ConstByteRange remaining;
        infra::AutoResetFunction<void(GattResult)> onDiscoveryDone;
        infra::AutoResetFunction<void()> onSendCompletion;
        infra::Function<void(infra::ConstByteRange data)> dataReceived;
    };
}

#endif
