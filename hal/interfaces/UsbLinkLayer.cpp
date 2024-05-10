#include "hal/interfaces/UsbLinkLayer.hpp"

namespace hal
{
    UsbPipe::UsbPipe(UsbHostLinkLayer& host, uint8_t pipe, uint8_t endPoint, uint8_t address, UsbSpeed speed, UsbEndPointType type, uint16_t maxPacketSize)
        : host(host)
        , pipe(pipe)
    {
        host.Open(pipe, endPoint, address, speed, type, maxPacketSize);
    }

    UsbPipe::~UsbPipe()
    {
        host.Close(pipe);
    }

    void UsbPipe::SubmitUsbRequestBlock(UsbHostLinkLayer::Direction direction, UsbEndPointType type, UsbHostLinkLayer::Pid token, infra::ByteRange buffer, bool ping)
    {
        host.SubmitUsbRequestBlock(pipe, direction, type, token, buffer, ping);
    }

    UsbHostLinkLayer::UsbRequestBlockState UsbPipe::RequestBlockState()
    {
        return host.RequestBlockState(pipe);
    }

    uint32_t UsbPipe::LastTransferSize()
    {
        return host.LastTransferSize(pipe);
    }

    void UsbPipe::SetToggle(bool toggle)
    {
        host.SetToggle(pipe, toggle);
    }

    bool UsbPipe::Toggle()
    {
        return host.Toggle(pipe);
    }
}
