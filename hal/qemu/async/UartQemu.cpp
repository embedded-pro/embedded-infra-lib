#include "hal/qemu/async/UartQemu.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace hal
{
    namespace
    {
        constexpr uint32_t lcrH8n1 = 0x70u;

        void ComputePl011Divisors(uint32_t uartClockHz, uint32_t baudrate, uint32_t& ibrd, uint32_t& fbrd)
        {
            uint32_t brd64 = (uartClockHz * 4u + baudrate / 2u) / baudrate;
            ibrd = brd64 >> 6u;
            fbrd = brd64 & 0x3Fu;
        }
    }

    UartQemu::UartQemu(uintptr_t base, int32_t irqNumber, uint32_t baudrate, uint32_t uartClockHz)
        : uart(*reinterpret_cast<Pl011Registers*>(base))
    {
        Init(baudrate, uartClockHz);
        Register(irqNumber);
    }

    void UartQemu::Init(uint32_t baudrate, uint32_t uartClockHz)
    {
        uint32_t ibrd = 0u;
        uint32_t fbrd = 0u;
        ComputePl011Divisors(uartClockHz, baudrate, ibrd, fbrd);
        uart.cr = 0u;
        uart.ibrd = ibrd;
        uart.fbrd = fbrd;
        uart.lcr_h = lcrH8n1;
        uart.imsc = 0u;
        uart.cr = crUarten | crTxe | crRxe;
    }

    void UartQemu::SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion)
    {
        sendBuffer = data;
        transferDataComplete = actionOnCompletion;
        uart.imsc |= imscTxim;
    }

    void UartQemu::ReceiveData(infra::Function<void(infra::ConstByteRange)> dataReceived)
    {
        onReceived = dataReceived;
        uart.imsc |= imscRxim;
    }

    void UartQemu::Invoke()
    {
        uint32_t status = uart.mis;

        if ((status & imscRxim) != 0u)
        {
            uint8_t byte = static_cast<uint8_t>(uart.dr);
            if (onReceived)
            {
                infra::EventDispatcher::Instance().Schedule([this, byte]()
                    {
                        receiveBuffer[0] = byte;
                        onReceived(infra::MakeRange(receiveBuffer.data(), receiveBuffer.data() + 1));
                    });
            }
        }

        if ((status & imscTxim) != 0u)
        {
            if (!sendBuffer.empty())
            {
                uart.dr = sendBuffer.front();
                sendBuffer.pop_front();
            }
            else
            {
                uart.imsc &= ~imscTxim;
                if (transferDataComplete)
                {
                    infra::EventDispatcher::Instance().Schedule([this]()
                        {
                            transferDataComplete();
                        });
                }
            }
        }

        uart.icr = 0xFFFFu;
    }
}
