#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "hal/qemu/default_init/SystemInit.hpp"
#include "hal/qemu/sync/Pl011Registers.hpp"
#include <cstdint>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern "C" void HardFault_Handler();
extern "C" void MemManage_Handler();
extern "C" void BusFault_Handler();
extern "C" void UsageFault_Handler();

extern "C" void __libc_init_array();
int main(int argc, char** argv);

extern "C" void Reset_Handler()
{
    uint32_t* src = &_sidata;
    for (uint32_t* dst = &_sdata; dst < &_edata;)
        *dst++ = *src++;

    for (uint32_t* dst = &_sbss; dst < &_ebss;)
        *dst++ = 0u;

    hal::qemu::SystemInit();
    __libc_init_array();

    static char programName[] = "qemu";
    static char* argv[] = { programName, nullptr };
    int const exitCode = main(1, argv);

    // SYS_EXIT_EXTENDED is required on Cortex-M targets to report exitCode;
    // plain SYS_EXIT ignores the parameter block and always reports failure.
    static uint32_t exitBlock[2] = { 0x20026u, 0u };
    exitBlock[1] = static_cast<uint32_t>(exitCode);
    hal::cortex::SemihostingCall(hal::cortex::SemihostingOperation::exitExtended,
        const_cast<uint32_t*>(exitBlock));
    while (true)
    {}
}

extern "C" __attribute__((weak)) void NMI_Handler()
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

namespace
{
    void Dispatch(int32_t irq)
    {
        if (hal::cortex::InterruptTable::InstanceSet())
            hal::cortex::InterruptTable::Instance().Invoke(irq);
    }
}

extern "C" void SysTick_Handler()
{
    Dispatch(hal::cortex::sysTickIrq);
}

extern "C" void UART0_IRQHandler()
{
    Dispatch(hal::uart0IrqNumber);
}

// CMSDK APB Timer 0 at 0x40000000 raises IRQ8 (exception 24) on MPS2-AN386.
constexpr int32_t timer0IrqNumber = 8;

extern "C" void TIMER0_IRQHandler()
{
    Dispatch(timer0IrqNumber);
}

// Vector entries are function pointer values; cast through uintptr_t avoids
// the -Wpointer-to-int-cast diagnostic on targets where sizeof(void*) == sizeof(uint32_t).
#define VEC(fn) static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn))

__attribute__((section(".isr_vector"), used))
const uint32_t vectorTable[] = {
    VEC(&_estack),
    VEC(Reset_Handler),         // exc  1
    VEC(NMI_Handler),           // exc  2
    VEC(HardFault_Handler),     // exc  3
    VEC(MemManage_Handler),     // exc  4
    VEC(BusFault_Handler),      // exc  5
    VEC(UsageFault_Handler),    // exc  6
    0u,                         // exc  7
    0u,                         // exc  8
    0u,                         // exc  9
    0u,                         // exc 10
    VEC(SVC_Handler),           // exc 11
    0u,                         // exc 12
    0u,                         // exc 13
    VEC(PendSV_Handler),        // exc 14
    VEC(SysTick_Handler),       // exc 15
    0u,                         // exc 16  IRQ0
    VEC(UART0_IRQHandler),      // exc 17  IRQ1
    0u,                         // exc 18  IRQ2
    0u,                         // exc 19  IRQ3
    0u,                         // exc 20  IRQ4
    0u,                         // exc 21  IRQ5
    0u,                         // exc 22  IRQ6
    0u,                         // exc 23  IRQ7
    VEC(TIMER0_IRQHandler),     // exc 24  IRQ8  CMSDK APB Timer 0
};

#undef VEC
