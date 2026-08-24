#include "hal/cortex_m/InterruptCortex.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

#ifndef EMIL_NVIC_PRIO_BITS
#if defined(__ARM_ARCH_6M__)
#define EMIL_NVIC_PRIO_BITS 2u
#else
#define EMIL_NVIC_PRIO_BITS 3u
#endif
#endif

namespace
{
    constexpr uintptr_t nvicIser0 = 0xE000E100u;
    constexpr uintptr_t nvicIcer0 = 0xE000E180u;
    constexpr uintptr_t nvicIcpr0 = 0xE000E280u;
    constexpr uintptr_t nvicIpr0 = 0xE000E400u;

    constexpr uintptr_t scbIcsr = 0xE000ED04u;
    constexpr uint32_t scbIcsrVectactiveMsk = 0x1FFu;
    constexpr uint32_t scbIcsrPendstclr = 1u << 25;

    constexpr uintptr_t systCsr = 0xE000E010u;
    constexpr uint32_t systTickint = 1u << 1;

    volatile uint32_t& Reg32(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }

    void Dmb()
    {
        __asm volatile("dmb" ::: "memory");
    }

    void Dsb()
    {
        __asm volatile("dsb" ::: "memory");
    }

    uint32_t IrqIndex(int32_t irq)
    {
        return static_cast<uint32_t>(irq);
    }

    volatile uint32_t& IrqBitBand(uintptr_t base, int32_t irq)
    {
        return Reg32(base + (IrqIndex(irq) / 32u) * 4u);
    }

    uint32_t IrqBit(int32_t irq)
    {
        return 1u << (IrqIndex(irq) % 32u);
    }

    uint8_t PriorityByte(hal::cortex::InterruptPriority priority)
    {
        constexpr uint8_t highestLevel = (1u << EMIL_NVIC_PRIO_BITS) - 1u;
        auto level = std::min(static_cast<uint8_t>(priority), highestLevel);
        return static_cast<uint8_t>(level << (8u - EMIL_NVIC_PRIO_BITS));
    }

    void SetNvicPriority(int32_t irq, uint8_t priorityByte)
    {
#if defined(__ARM_ARCH_6M__)
        auto index = IrqIndex(irq);
        auto shift = (index % 4u) * 8u;
        auto& word = Reg32(nvicIpr0 + (index / 4u) * 4u);
        word = (word & ~(0xFFu << shift)) | (static_cast<uint32_t>(priorityByte) << shift);
#else
        *(reinterpret_cast<volatile uint8_t*>(nvicIpr0) + IrqIndex(irq)) = priorityByte;
#endif
        Dsb();
    }

    void EnableIrq(int32_t irq, hal::cortex::InterruptPriority priority)
    {
        if (irq >= hal::cortex::firstExternalIrq)
        {
            SetNvicPriority(irq, PriorityByte(priority));
            IrqBitBand(nvicIser0, irq) = IrqBit(irq);
        }
        else if (irq == hal::cortex::sysTickIrq)
            Reg32(systCsr) |= systTickint;
    }

    void DisableIrq(int32_t irq)
    {
        if (irq >= hal::cortex::firstExternalIrq)
        {
            IrqBitBand(nvicIcer0, irq) = IrqBit(irq);
            Dsb();
        }
        else if (irq == hal::cortex::sysTickIrq)
        {
            Reg32(systCsr) &= ~systTickint;
            Reg32(scbIcsr) = scbIcsrPendstclr;
        }
    }

    void ClearPendingIrq(int32_t irq)
    {
        if (irq >= hal::cortex::firstExternalIrq)
            IrqBitBand(nvicIcpr0, irq) = IrqBit(irq);
        else if (irq == hal::cortex::sysTickIrq)
            Reg32(scbIcsr) = scbIcsrPendstclr;
    }
}

namespace hal::cortex
{
    int32_t ActiveInterrupt()
    {
        return static_cast<int32_t>(Reg32(scbIcsr) & scbIcsrVectactiveMsk) - static_cast<int32_t>(exceptionOffset);
    }

    InterruptHandler::InterruptHandler() = default;

    InterruptHandler::InterruptHandler(InterruptHandler&& other) noexcept
        : irq(other.irq)
        , priority(other.priority)
    {
        if (irq)
            InterruptTable::Instance().TakeOverHandler(*irq, *this, other);

        other.irq = std::nullopt;
    }

    InterruptHandler& InterruptHandler::operator=(InterruptHandler&& other) noexcept
    {
        if (irq)
            InterruptTable::Instance().UnregisterHandler(*irq, *this);

        irq = other.irq;
        priority = other.priority;

        if (irq)
            InterruptTable::Instance().TakeOverHandler(*irq, *this, other);

        other.irq = std::nullopt;
        return *this;
    }

    InterruptHandler::~InterruptHandler()
    {
        Unregister();
    }

    void InterruptHandler::Register(int32_t irqNumber, InterruptPriority newPriority)
    {
        really_assert(!irq);

        irq = irqNumber;
        priority = newPriority;
        InterruptTable::Instance().RegisterHandler(irqNumber, *this, newPriority);
    }

    void InterruptHandler::Unregister()
    {
        if (irq)
            InterruptTable::Instance().UnregisterHandler(*irq, *this);

        irq = std::nullopt;
    }

    void InterruptHandler::ClearPending()
    {
        if (irq)
            ClearPendingIrq(*irq);
    }

    bool InterruptHandler::Registered() const
    {
        return irq.has_value();
    }

    int32_t InterruptHandler::Irq() const
    {
        really_assert(irq.has_value());
        return *irq;
    }

    InterruptPriority InterruptHandler::Priority() const
    {
        return priority;
    }

    InterruptTable::InterruptTable(infra::MemoryRange<InterruptHandler*> table)
        : table(table)
    {
        std::fill(table.begin(), table.end(), nullptr);
    }

    void InterruptTable::Invoke(int32_t irq)
    {
        Dmb();

        auto index = Index(irq);
        really_assert(table[index] != nullptr);
        table[index]->Invoke();
    }

    InterruptHandler* InterruptTable::Handler(int32_t irq) const
    {
        return table[Index(irq)];
    }

    void InterruptTable::RegisterHandler(int32_t irq, InterruptHandler& handler, InterruptPriority priority)
    {
        auto index = Index(irq);
        really_assert(table[index] == nullptr);

        table[index] = &handler;
        Dsb();
        EnableIrq(irq, priority);
    }

    void InterruptTable::UnregisterHandler(int32_t irq, InterruptHandler& handler)
    {
        auto index = Index(irq);
        really_assert(table[index] == &handler);

        DisableIrq(irq);
        Dsb();
        table[index] = nullptr;
    }

    void InterruptTable::TakeOverHandler(int32_t irq, InterruptHandler& handler, const InterruptHandler& previous)
    {
        auto index = Index(irq);
        really_assert(table[index] == &previous);

        table[index] = &handler;
        Dsb();
    }

    std::size_t InterruptTable::Index(int32_t irq) const
    {
        auto index = static_cast<std::size_t>(irq + static_cast<int32_t>(exceptionOffset));
        really_assert(index < table.size());
        return index;
    }

    DispatchedInterruptHandler::DispatchedInterruptHandler(int32_t irq, infra::Function<void()> onInvoke)
        : DispatchedInterruptHandler(irq, InterruptPriority::normal, onInvoke)
    {}

    DispatchedInterruptHandler::DispatchedInterruptHandler(int32_t irq, InterruptPriority priority, infra::Function<void()> onInvoke)
        : onInvoke(onInvoke)
    {
        Register(irq, priority);
    }

    void DispatchedInterruptHandler::Invoke()
    {
        DisableIrq(Irq());
        really_assert(!pending);
        pending = true;

        auto irq = Irq();
        infra::EventDispatcher::Instance().Schedule([irq, this]()
            {
                InvokeScheduled(irq, *this);
            });
    }

    void DispatchedInterruptHandler::SetInvoke(infra::Function<void()> newOnInvoke)
    {
        onInvoke = newOnInvoke;
    }

    void DispatchedInterruptHandler::InvokeScheduled(int32_t irq, DispatchedInterruptHandler& handler)
    {
        if (InterruptTable::Instance().Handler(irq) != &handler)
            return;

        infra::Function<void()> invoke = handler.onInvoke;
        invoke();

        if (InterruptTable::Instance().Handler(irq) != &handler)
            return;

        handler.pending = false;

        if (handler.onInvoke)
            EnableIrq(irq, handler.Priority());
    }

    ImmediateInterruptHandler::ImmediateInterruptHandler(int32_t irq, infra::Function<void()> onInvoke)
        : ImmediateInterruptHandler(irq, InterruptPriority::normal, onInvoke)
    {}

    ImmediateInterruptHandler::ImmediateInterruptHandler(int32_t irq, InterruptPriority priority, infra::Function<void()> onInvoke)
        : onInvoke(onInvoke)
    {
        Register(irq, priority);
    }

    void ImmediateInterruptHandler::Invoke()
    {
        onInvoke();
    }
}
