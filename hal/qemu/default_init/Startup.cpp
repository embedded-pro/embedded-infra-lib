#include "hal/qemu/cortex/InterruptCortex.hpp"
#include "hal/qemu/default_init/SystemInit.hpp"
#include <cstdint>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern "C" void __libc_init_array();
extern "C" int main();

extern "C" void Reset_Handler()
{
    uint32_t* src = &_sidata;
    for (uint32_t* dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;

    for (uint32_t* dst = &_sbss; dst < &_ebss;)
        *dst++ = 0u;

    __libc_init_array();
    hal::qemu::SystemInit();
    main();
    while (true)
    {}
}

extern "C" __attribute__((weak)) void NMI_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void HardFault_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void MemManage_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void BusFault_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void UsageFault_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void SVC_Handler()
{
    while (true)
    {}
}

extern "C" __attribute__((weak)) void PendSV_Handler()
{
    while (true)
    {}
}

extern "C" void SysTick_Handler()
{
    hal::cortex::InterruptCortexDispatch(-1);
}

extern "C" void UART0_IRQHandler()
{
    hal::cortex::InterruptCortexDispatch(hal::uart0IrqNumber);
}

__attribute__((section(".isr_vector"), used))
const uint32_t vectorTable[] = {
    reinterpret_cast<uint32_t>(&_estack),
    reinterpret_cast<uint32_t>(&Reset_Handler),
    reinterpret_cast<uint32_t>(&NMI_Handler),
    reinterpret_cast<uint32_t>(&HardFault_Handler),
    reinterpret_cast<uint32_t>(&MemManage_Handler),
    reinterpret_cast<uint32_t>(&BusFault_Handler),
    reinterpret_cast<uint32_t>(&UsageFault_Handler),
    0u,
    0u,
    0u,
    0u,
    reinterpret_cast<uint32_t>(&SVC_Handler),
    0u,
    0u,
    reinterpret_cast<uint32_t>(&PendSV_Handler),
    reinterpret_cast<uint32_t>(&SysTick_Handler),
    0u,
    reinterpret_cast<uint32_t>(&UART0_IRQHandler),
};
