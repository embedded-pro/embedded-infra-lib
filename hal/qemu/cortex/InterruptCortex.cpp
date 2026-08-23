#include "hal/qemu/cortex/InterruptCortex.hpp"
#include <array>
#include <cstdint>

namespace hal::cortex
{
    namespace
    {
        constexpr uintptr_t nvicIser = 0xE000E100;
        constexpr uintptr_t nvicIcer = 0xE000E180;
        constexpr uintptr_t nvicIpr = 0xE000E400;

        std::array<InterruptHandler*, 64> handlers{};
        InterruptHandler* sysTickHandler{ nullptr };
    }

    void InterruptCortexRegisterHandler(int32_t irqNumber, InterruptHandler& handler)
    {
        if (irqNumber == -1)
            sysTickHandler = &handler;
        else
            handlers[static_cast<std::size_t>(irqNumber)] = &handler;
    }

    void InterruptCortexDispatch(int32_t irqNumber)
    {
        if (irqNumber == -1)
        {
            if (sysTickHandler != nullptr)
                sysTickHandler->Invoke();
        }
        else
        {
            auto index = static_cast<std::size_t>(irqNumber);
            if (handlers[index] != nullptr)
                handlers[index]->Invoke();
        }
    }

    void EnableInterrupt(int32_t irqNumber)
    {
        auto index = static_cast<uint32_t>(irqNumber);
        *reinterpret_cast<volatile uint32_t*>(nvicIser + (index / 32) * 4) = 1u << (index % 32);
    }

    void DisableInterrupt(int32_t irqNumber)
    {
        auto index = static_cast<uint32_t>(irqNumber);
        *reinterpret_cast<volatile uint32_t*>(nvicIcer + (index / 32) * 4) = 1u << (index % 32);
    }

    void SetInterruptPriority(int32_t irqNumber, uint8_t priority)
    {
        auto index = static_cast<uint32_t>(irqNumber);
        reinterpret_cast<volatile uint8_t*>(nvicIpr)[index] = priority;
    }
}
