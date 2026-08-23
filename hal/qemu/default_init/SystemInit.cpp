#include "hal/qemu/default_init/SystemInit.hpp"
#include <cstdint>

namespace hal::qemu
{
    void SystemInit()
    {
#if defined(EMIL_TARGET_CORTEX_M4) || defined(EMIL_TARGET_CORTEX_M7)
        *reinterpret_cast<volatile uint32_t*>(0xE000ED88u) |= (0xFu << 20);
        __asm volatile("dsb");
        __asm volatile("isb");
#endif
    }
}
