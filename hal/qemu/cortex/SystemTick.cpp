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

        constexpr uint32_t csrEnable = 1u << 0;
        constexpr uint32_t csrTickint = 1u << 1;
        constexpr uint32_t csrClksource = 1u << 2;
    }

    SystemTick::SystemTick(uint32_t coreClockHz, infra::Duration tickDuration)
        : reload((coreClockHz / 1000000u) *
                     static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::microseconds>(tickDuration).count()) -
                 1u)
    {}

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
