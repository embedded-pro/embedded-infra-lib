#include "hal/qemu/sync/SynchronousUartQemu.hpp"

namespace hal
{
    namespace
    {
        constexpr uint32_t lcrH8n1   = 0x70u;
        constexpr uint32_t ibrdValue  = 13u;
        constexpr uint32_t fbrdValue  = 1u;
    }

    SynchronousUartQemu::SynchronousUartQemu(uintptr_t base, uint32_t baudrate)
        : uart(*reinterpret_cast<Pl011Registers*>(base))
    {
        Init(baudrate);
    }

    void SynchronousUartQemu::Init(uint32_t baudrate)
    {
        uart.cr = 0u;
        uart.ibrd = ibrdValue;
        uart.fbrd = fbrdValue;
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
