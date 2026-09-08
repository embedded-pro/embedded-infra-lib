#include "hal/cortex_m/InterruptCortex.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "hal/qemu/default_init/SystemInit.hpp"
#include "infra/util/ReallyAssert.hpp"
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

extern "C" void DefaultHandler()
{
    really_assert(hal::cortex::InterruptTable::InstanceSet());
    hal::cortex::InterruptTable::Instance().Invoke(hal::cortex::ActiveInterrupt());
}

#define EMIL_DEFAULT_HANDLER(name) \
    extern "C" [[gnu::weak, gnu::alias("DefaultHandler")]] void name()

EMIL_DEFAULT_HANDLER(NMI_Handler);
EMIL_DEFAULT_HANDLER(SVC_Handler);
EMIL_DEFAULT_HANDLER(DebugMon_Handler);
EMIL_DEFAULT_HANDLER(PendSV_Handler);
EMIL_DEFAULT_HANDLER(SysTick_Handler);

EMIL_DEFAULT_HANDLER(IRQ0_Handler);
EMIL_DEFAULT_HANDLER(IRQ1_Handler);
EMIL_DEFAULT_HANDLER(IRQ2_Handler);
EMIL_DEFAULT_HANDLER(IRQ3_Handler);
EMIL_DEFAULT_HANDLER(IRQ4_Handler);
EMIL_DEFAULT_HANDLER(IRQ5_Handler);
EMIL_DEFAULT_HANDLER(IRQ6_Handler);
EMIL_DEFAULT_HANDLER(IRQ7_Handler);
EMIL_DEFAULT_HANDLER(IRQ8_Handler);
EMIL_DEFAULT_HANDLER(IRQ9_Handler);
EMIL_DEFAULT_HANDLER(IRQ10_Handler);
EMIL_DEFAULT_HANDLER(IRQ11_Handler);
EMIL_DEFAULT_HANDLER(IRQ12_Handler);
EMIL_DEFAULT_HANDLER(IRQ13_Handler);
EMIL_DEFAULT_HANDLER(IRQ14_Handler);
EMIL_DEFAULT_HANDLER(IRQ15_Handler);
EMIL_DEFAULT_HANDLER(IRQ16_Handler);
EMIL_DEFAULT_HANDLER(IRQ17_Handler);
EMIL_DEFAULT_HANDLER(IRQ18_Handler);
EMIL_DEFAULT_HANDLER(IRQ19_Handler);
EMIL_DEFAULT_HANDLER(IRQ20_Handler);
EMIL_DEFAULT_HANDLER(IRQ21_Handler);
EMIL_DEFAULT_HANDLER(IRQ22_Handler);
EMIL_DEFAULT_HANDLER(IRQ23_Handler);
EMIL_DEFAULT_HANDLER(IRQ24_Handler);
EMIL_DEFAULT_HANDLER(IRQ25_Handler);
EMIL_DEFAULT_HANDLER(IRQ26_Handler);
EMIL_DEFAULT_HANDLER(IRQ27_Handler);
EMIL_DEFAULT_HANDLER(IRQ28_Handler);
EMIL_DEFAULT_HANDLER(IRQ29_Handler);
EMIL_DEFAULT_HANDLER(IRQ30_Handler);
EMIL_DEFAULT_HANDLER(IRQ31_Handler);

#undef EMIL_DEFAULT_HANDLER

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
    VEC(DebugMon_Handler),
    0u,
    VEC(PendSV_Handler),
    VEC(SysTick_Handler),
    VEC(IRQ0_Handler),
    VEC(IRQ1_Handler),
    VEC(IRQ2_Handler),
    VEC(IRQ3_Handler),
    VEC(IRQ4_Handler),
    VEC(IRQ5_Handler),
    VEC(IRQ6_Handler),
    VEC(IRQ7_Handler),
    VEC(IRQ8_Handler),
    VEC(IRQ9_Handler),
    VEC(IRQ10_Handler),
    VEC(IRQ11_Handler),
    VEC(IRQ12_Handler),
    VEC(IRQ13_Handler),
    VEC(IRQ14_Handler),
    VEC(IRQ15_Handler),
    VEC(IRQ16_Handler),
    VEC(IRQ17_Handler),
    VEC(IRQ18_Handler),
    VEC(IRQ19_Handler),
    VEC(IRQ20_Handler),
    VEC(IRQ21_Handler),
    VEC(IRQ22_Handler),
    VEC(IRQ23_Handler),
    VEC(IRQ24_Handler),
    VEC(IRQ25_Handler),
    VEC(IRQ26_Handler),
    VEC(IRQ27_Handler),
    VEC(IRQ28_Handler),
    VEC(IRQ29_Handler),
    VEC(IRQ30_Handler),
    VEC(IRQ31_Handler),
};

#undef VEC
