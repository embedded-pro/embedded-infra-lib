#include "hal/qemu/cortex/InterruptCortex.hpp"
#include "hal/qemu/cortex/Semihosting.hpp"
#include "hal/qemu/default_init/SystemInit.hpp"
#include "hal/qemu/sync/Pl011Registers.hpp"
#include <cstdint>

extern uint32_t _estack;
extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;
extern uint32_t _sbss;
extern uint32_t _ebss;

extern "C" void __libc_init_array();
int main(int argc, char** argv);

// Required by the C++ ABI for global object lifetime management.
// Provided here because -nostartfiles suppresses crtbegin.o/crti.o.
extern "C"
{
    __attribute__((weak)) void* __dso_handle = nullptr;

    void _init()
    {}
}

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

    // A32/T32 SYS_EXIT (0x18) only accepts ADP_Stopped_ApplicationExit as a
    // direct register value and cannot convey an exit code; SYS_EXIT_EXTENDED
    // (0x20) takes a pointer to this block so QEMU can report exitCode back
    // to the host as its own process exit status.
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

// Vector entries are function pointer values; cast through uintptr_t avoids
// the -Wpointer-to-int-cast diagnostic on targets where sizeof(void*) == sizeof(uint32_t).
#define VEC(fn) static_cast<uint32_t>(reinterpret_cast<uintptr_t>(fn))

__attribute__((section(".isr_vector"), used))
const uint32_t vectorTable[] = {
    VEC(&_estack),
    VEC(Reset_Handler),
    VEC(NMI_Handler),
    VEC(HardFault_Handler),
    VEC(MemManage_Handler),
    VEC(BusFault_Handler),
    VEC(UsageFault_Handler),
    0u,
    0u,
    0u,
    0u,
    VEC(SVC_Handler),
    0u,
    0u,
    VEC(PendSV_Handler),
    VEC(SysTick_Handler),
    0u,
    VEC(UART0_IRQHandler),
};

#undef VEC
