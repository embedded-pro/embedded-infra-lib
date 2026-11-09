#ifndef HAL_QEMU_CMSDK_CMSDK_UART_REGISTERS_HPP
#define HAL_QEMU_CMSDK_CMSDK_UART_REGISTERS_HPP

#include <cstddef>
#include <cstdint>

namespace hal
{
    struct CmsdkUartRegisters
    {
        volatile uint32_t data;
        volatile uint32_t state;
        volatile uint32_t control;
        volatile uint32_t intStatus;
        volatile uint32_t bauddiv;
    };

    static_assert(offsetof(CmsdkUartRegisters, bauddiv) == 0x10);

    constexpr uintptr_t cmsdkUart0BaseAddress = 0x40004000;
    constexpr uintptr_t cmsdkUart1BaseAddress = 0x40005000;
    constexpr uintptr_t cmsdkUart2BaseAddress = 0x40006000;

    constexpr uint32_t uartStateTxFull = 1u << 0;
    constexpr uint32_t uartStateRxFull = 1u << 1;
    constexpr uint32_t uartControlTxEnable = 1u << 0;
    constexpr uint32_t uartControlRxEnable = 1u << 1;
    constexpr uint32_t uartDataMask = 0xffu;
}

#endif
