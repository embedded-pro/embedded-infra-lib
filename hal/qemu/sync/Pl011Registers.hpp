#ifndef HAL_QEMU_SYNC_PL011_REGISTERS_HPP
#define HAL_QEMU_SYNC_PL011_REGISTERS_HPP

#include <cstdint>

namespace hal
{
    struct Pl011Registers
    {
        volatile uint32_t dr;
        volatile uint32_t rsr;
        volatile uint32_t reserved[4];
        volatile uint32_t fr;
        volatile uint32_t reserved2;
        volatile uint32_t ilpr;
        volatile uint32_t ibrd;
        volatile uint32_t fbrd;
        volatile uint32_t lcr_h;
        volatile uint32_t cr;
        volatile uint32_t ifls;
        volatile uint32_t imsc;
        volatile uint32_t ris;
        volatile uint32_t mis;
        volatile uint32_t icr;
    };

    constexpr uintptr_t pl011BaseAddress = 0x09000000;

    constexpr uint32_t frTxff = 1u << 5;
    constexpr uint32_t frRxfe = 1u << 4;
    constexpr uint32_t imscRxim = 1u << 4;
    constexpr uint32_t imscTxim = 1u << 5;
    constexpr uint32_t crUarten = 1u << 0;
    constexpr uint32_t crTxe = 1u << 8;
    constexpr uint32_t crRxe = 1u << 9;

    constexpr int32_t uart0IrqNumber = 1;

    inline Pl011Registers& Pl011()
    {
        return *reinterpret_cast<Pl011Registers*>(pl011BaseAddress);
    }
}

#endif
