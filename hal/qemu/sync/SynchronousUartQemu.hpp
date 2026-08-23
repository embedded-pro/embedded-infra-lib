#ifndef HAL_QEMU_SYNC_SYNCHRONOUS_UART_QEMU_HPP
#define HAL_QEMU_SYNC_SYNCHRONOUS_UART_QEMU_HPP

#include "hal/qemu/sync/Pl011Registers.hpp"
#include "hal/synchronous_interfaces/SynchronousSerialCommunication.hpp"

namespace hal
{
    class SynchronousUartQemu
        : public hal::SynchronousSerialCommunication
    {
    public:
        explicit SynchronousUartQemu(uintptr_t base = pl011BaseAddress,
            uint32_t baudrate = 115200,
            uint32_t uartClockHz = pl011ClockHz);

        void SendData(infra::ConstByteRange data) override;
        bool ReceiveData(infra::ByteRange data) override;

    private:
        void Init(uint32_t baudrate, uint32_t uartClockHz);

    private:
        Pl011Registers& uart;
    };
}

#endif
