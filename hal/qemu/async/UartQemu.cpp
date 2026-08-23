#include "hal/qemu/async/UartQemu.hpp"
#include "infra/event/EventDispatcher.hpp"

namespace hal
{
    namespace
    {
        constexpr uint32_t lcrH8n1  = 0x70u;
        constexpr uint32_t ibrdValue = 13u;
        constexpr uint32_t fbrdValue = 1u;
    }

    UartQemu::UartQemu(uintptr_t base, int32_t irqNumber, uint32_t baudrate)
        : uart(*reinterpret_cast<Pl011Registers*>(base))
        , irqNumber(irqNumber)
    {
        cortex::InterruptCortexRegisterHandler(irqNumber, *this);
        Init(baudrate);
    }

    void UartQemu::Init(uint32_t baudrate)
    {
        uart.cr = 0u;
        uart.ibrd = ibrdValue;
        uart.fbrd = fbrdValue;
        uart.lcr_h = lcrH8n1;
        uart.imsc = 0u;
        uart.cr = crUarten | crTxe | crRxe;
        cortex::EnableInterrupt(irqNumber);
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
            receiveBuffer[0] = static_cast<uint8_t>(uart.dr);
            if (onReceived)
            {
                infra::ConstByteRange received{ receiveBuffer.data(), receiveBuffer.data() + 1 };
                infra::EventDispatcher::Instance().Schedule([this, received]()
                    {
                        onReceived(received);
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
