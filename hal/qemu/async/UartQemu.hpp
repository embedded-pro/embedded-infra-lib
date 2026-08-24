#ifndef HAL_QEMU_ASYNC_UART_QEMU_HPP
#define HAL_QEMU_ASYNC_UART_QEMU_HPP

#include "hal/interfaces/SerialCommunication.hpp"
#include "hal/qemu/cortex/InterruptCortex.hpp"
#include "hal/qemu/sync/Pl011Registers.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include <array>
#include <cstdint>

namespace hal
{
    class UartQemu
        : public hal::SerialCommunication
        , private cortex::InterruptHandler
    {
    public:
        UartQemu(uintptr_t base = pl011BaseAddress,
            int32_t irqNumber = uart0IrqNumber,
            uint32_t baudrate = 115200,
            uint32_t uartClockHz = pl011ClockHz);

        void SendData(infra::ConstByteRange data, infra::Function<void()> actionOnCompletion) override;
        void ReceiveData(infra::Function<void(infra::ConstByteRange)> dataReceived) override;

    private:
        void Invoke() override;
        void Init(uint32_t baudrate, uint32_t uartClockHz);

    private:
        Pl011Registers& uart;
        int32_t irqNumber;
        infra::ConstByteRange sendBuffer;
        infra::Function<void()> transferDataComplete;
        infra::Function<void(infra::ConstByteRange)> onReceived;
        std::array<uint8_t, 1> receiveBuffer{};
    };
}

#endif
