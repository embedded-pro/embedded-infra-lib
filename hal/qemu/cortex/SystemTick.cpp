#include "hal/qemu/cortex/SystemTick.hpp"
#include <chrono>
#include <cstdint>

namespace hal::cortex
{
    namespace
    {
        constexpr uintptr_t systCsr = 0xE000E010;
        constexpr uintptr_t systRvr = 0xE000E014;
        constexpr uintptr_t systCvr = 0xE000E018;

        constexpr uint32_t csrEnable    = 1u << 0;
        constexpr uint32_t csrTickint   = 1u << 1;
        constexpr uint32_t csrClksource = 1u << 2;
    }

    SystemTick::SystemTick(uint32_t coreClockHz, infra::Duration tickDuration)
    {
        auto ns = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(tickDuration).count());
        auto cycles = static_cast<uint64_t>(coreClockHz) * ns / 1000000000u;
        reload = static_cast<uint32_t>(cycles - 1u);
    }

    void SystemTick::Enable()
    {
        *reinterpret_cast<volatile uint32_t*>(systRvr) = reload;
        *reinterpret_cast<volatile uint32_t*>(systCvr) = 0u;
        *reinterpret_cast<volatile uint32_t*>(systCsr) = csrClksource | csrTickint | csrEnable;
    }

    void SystemTick::Disable()
    {
        *reinterpret_cast<volatile uint32_t*>(systCsr) = 0u;
    }
}
