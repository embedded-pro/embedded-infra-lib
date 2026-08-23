#include "hal/qemu/cortex/Reset.hpp"
#include <cstdint>

namespace hal::cortex
{
    namespace
    {
        constexpr uintptr_t aircrAddress = 0xE000ED0Cu;
        constexpr uint32_t aircrResetValue = (0x5FAu << 16) | (1u << 2);
    }

    void Reset::ResetModule()
    {
        *reinterpret_cast<volatile uint32_t*>(aircrAddress) = aircrResetValue;
    }
}
