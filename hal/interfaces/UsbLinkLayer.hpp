#ifndef HAL_USB_LINK_LAYER_HPP
#define HAL_USB_LINK_LAYER_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/Observer.hpp"
#include <cstdint>

namespace hal
{
    enum class UsbSpeed : uint8_t
    {
        high,
        full,
        low
    };

    enum class UsbEndPointType : uint8_t
    {
        control = 0,
        isochronous = 1,
        bulk = 2,
        interrupt = 3
    };

    class UsbDeviceLinkLayer;

    class UsbDeviceFactory
    {
    public:
        UsbDeviceFactory() = default;
        UsbDeviceFactory(const UsbDeviceFactory& other) = delete;
        UsbDeviceFactory& operator=(const UsbDeviceFactory& other) = delete;

        virtual void Create(UsbDeviceLinkLayer& linkLayer) = 0;
        virtual void Destroy() = 0;

    protected:
        ~UsbDeviceFactory() = default;
    };

    class UsbDeviceLinkLayerObserver
        : public infra::SingleObserver<UsbDeviceLinkLayerObserver, UsbDeviceLinkLayer>
    {
    protected:
        UsbDeviceLinkLayerObserver(UsbDeviceLinkLayer& linkLayer)
            : infra::SingleObserver<UsbDeviceLinkLayerObserver, UsbDeviceLinkLayer>(linkLayer)
        {}

        ~UsbDeviceLinkLayerObserver() = default;

    public:
        virtual void SetupStage(infra::ConstByteRange setup) = 0;
        virtual void DataOutStage(uint8_t epnum, infra::ConstByteRange data) = 0;
        virtual void DataInStage(uint8_t epnum, infra::ConstByteRange data) = 0;

        virtual void Suspend() = 0;
        virtual void Resume() = 0;

        virtual void StartOfFrame() = 0;
        virtual void IsochronousInIncomplete(uint8_t epnum) = 0;
        virtual void IsochronousOutIncomplete(uint8_t epnum) = 0;
    };

    class UsbDeviceLinkLayer
        : public infra::Subject<UsbDeviceLinkLayerObserver>
    {
    public:
        UsbDeviceLinkLayer() = default;
        UsbDeviceLinkLayer(const UsbDeviceLinkLayer& other) = delete;
        UsbDeviceLinkLayer& operator=(const UsbDeviceLinkLayer& other) = delete;

    protected:
        ~UsbDeviceLinkLayer() = default;

    public:
        virtual void OpenEndPoint(uint8_t address, UsbEndPointType type, uint16_t maxPacketSize) = 0;
        virtual void CloseEndPoint(uint8_t address) = 0;
        virtual void FlushEndPoint(uint8_t address) = 0;
        virtual void StallEndPoint(uint8_t address) = 0;
        virtual void ClearStallEndPoint(uint8_t address) = 0;
        virtual bool IsStallEndPoint(uint8_t address) = 0;
        virtual void SetUsbAddress(uint8_t dev_addr) = 0;
        virtual void Transmit(uint8_t address, infra::ConstByteRange data) = 0;
        virtual void PrepareReceive(uint8_t address, infra::ConstByteRange data) = 0;
        virtual uint32_t GetReceiveDataSize(uint8_t address) = 0;
    };

    class UsbHostLinkLayer;

    class UsbHostLinkLayerObserver
        : public infra::SingleObserver<UsbHostLinkLayerObserver, UsbHostLinkLayer>
    {
    public:
        using infra::SingleObserver<UsbHostLinkLayerObserver, UsbHostLinkLayer>::SingleObserver;

        virtual void Connected() = 0;
        virtual void Disconnected() = 0;
        virtual void PortEnabled() = 0;
        virtual void PortDisabled() = 0;
        virtual void StartOfFrame() = 0;
    };

    class UsbHostLinkLayer
        : public infra::Subject<UsbHostLinkLayerObserver>
    {
    public:
        UsbHostLinkLayer() = default;
        UsbHostLinkLayer(const UsbHostLinkLayer& other) = delete;
        UsbHostLinkLayer& operator=(const UsbHostLinkLayer& other) = delete;

        enum struct Pid : uint8_t
        {
            setup,
            data
        };

        enum struct Direction : uint8_t
        {
            in,
            out
        };

        enum class UsbRequestBlockState : uint8_t
        {
            success,
            error,
            stall,
            notReady
        };

        virtual UsbSpeed Speed() = 0;
        virtual void ResetPort() = 0;

        virtual void Open(uint8_t pipe, uint8_t endPoint, uint8_t address, UsbSpeed speed, UsbEndPointType type, uint16_t maxPacketSize) = 0;
        virtual void Close(uint8_t pipe) = 0;
        virtual void Transmit(uint8_t pipe, UsbEndPointType type, Pid token, infra::ConstByteRange buffer, bool ping, const infra::Function<void(UsbRequestBlockState)>& onDone) = 0;
        virtual void Receive(uint8_t pipe, UsbEndPointType type, Pid token, bool ping, const infra::Function<void(infra::ConstByteRange, UsbRequestBlockState)>& onDone) = 0;
        virtual void SetToggle(uint8_t pipe, bool toggle) = 0;
        virtual bool Toggle(uint8_t pipe) = 0;
    };
}

#endif
