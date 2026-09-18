#ifndef SERVICES_NORDIC_UART_CENTRAL_HPP
#define SERVICES_NORDIC_UART_CENTRAL_HPP

#include "infra/util/AutoResetFunction.hpp"
#include "services/ble/GattClientCharacteristic.hpp"
#include "services/ble/profile/NordicUart.hpp"
#include <optional>

namespace services
{
    class NordicUartCentral
        : public NordicUart
        , private GattClientConnectionObserver
        , private GattClientCharacteristicUpdateObserver
    {
    public:
        enum class WriteMode : uint8_t
        {
            withResponse,
            withoutResponse
        };

        explicit NordicUartCentral(GattClientConnection& connection, WriteMode writeMode = WriteMode::withResponse);
        ~NordicUartCentral();

        GattRequestStatus Discover(const infra::Function<void(GattResult)>& onDone);

        void Close();

        bool IsOpen() const override;
        std::size_t MaxSendSize() const override;

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange data)> dataReceived) override;

    private:
        void ServiceDiscovered(const GattService& service) override;
        void IncludedServiceDiscovered(const GattIncludedService& includedService) override;
        void CharacteristicDiscovered(const GattCharacteristic& characteristic) override;
        void DescriptorDiscovered(const GattDescriptor& descriptor) override;
        void MtuChanged(uint16_t mtu) override;

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
