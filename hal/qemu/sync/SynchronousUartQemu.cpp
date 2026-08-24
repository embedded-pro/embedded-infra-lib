#include "hal/qemu/sync/SynchronousUartQemu.hpp"

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

    SynchronousUartQemu::SynchronousUartQemu(uintptr_t base, uint32_t baudrate, uint32_t uartClockHz)
        : uart(*reinterpret_cast<Pl011Registers*>(base))
    {
        Init(baudrate, uartClockHz);
    }

    void SynchronousUartQemu::Init(uint32_t baudrate, uint32_t uartClockHz)
    {
        uint32_t ibrd = 0u;
        uint32_t fbrd = 0u;
        ComputePl011Divisors(uartClockHz, baudrate, ibrd, fbrd);
        uart.cr = 0u;
        uart.ibrd = ibrd;
        uart.fbrd = fbrd;
        uart.lcr_h = lcrH8n1;
        uart.cr = crUarten | crTxe | crRxe;
    }

    void SynchronousUartQemu::SendData(infra::ConstByteRange data)
    {
        for (uint8_t byte : data)
        {
            while ((uart.fr & frTxff) != 0u)
            {}
            uart.dr = byte;
        }
    }

    bool SynchronousUartQemu::ReceiveData(infra::ByteRange data)
    {
        for (uint8_t& byte : data)
        {
            while ((uart.fr & frRxfe) != 0u)
            {}
            byte = static_cast<uint8_t>(uart.dr);
        }
        return true;
    }
}
