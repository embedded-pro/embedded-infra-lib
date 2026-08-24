#ifndef HAL_CORTEX_M_INTERRUPT_CORTEX_HPP
#define HAL_CORTEX_M_INTERRUPT_CORTEX_HPP

#include "infra/util/Function.hpp"
#include "infra/util/InterfaceConnector.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace hal::cortex
{
    // Exception numbers below zero identify core exceptions; zero and above identify
    // external interrupts. The exception number of an interrupt is its irq number plus 16.
    constexpr int32_t nmiIrq = -14;
    constexpr int32_t hardFaultIrq = -13;
    constexpr int32_t memManageIrq = -12;
    constexpr int32_t busFaultIrq = -11;
    constexpr int32_t usageFaultIrq = -10;
    constexpr int32_t svCallIrq = -5;
    constexpr int32_t pendSvIrq = -2;
    constexpr int32_t sysTickIrq = -1;

    constexpr int32_t firstExternalIrq = 0;
    constexpr std::size_t exceptionOffset = 16;

    // Priority values are logical levels: 0 is the highest, 7 the lowest. The value is
    // written to NVIC_IPR shifted left by (8 - EMIL_NVIC_PRIO_BITS).
    //
    // Implemented bits per family, which determines the effective range:
    //   ARMv6-M (M0/M0+)      : 2 bits, so levels 0-3; higher values saturate at 3.
    //   ARMv7-M (M3/M4/M7)    : 3 bits minimum by specification, so levels 0-7.
    //   ARMv8-M (M33)         : 3 bits minimum by specification, so levels 0-7.
    enum class InterruptPriority : uint8_t
    {
        highest = 0,
        high = 1,
        normal = 2,
        low = 4,
        lowest = 7,
    };

    int32_t ActiveInterrupt();

    class InterruptTable;

    class InterruptHandler
    {
    protected:
        InterruptHandler();
        InterruptHandler(const InterruptHandler&) = delete;
        InterruptHandler(InterruptHandler&& other) noexcept;
        InterruptHandler& operator=(const InterruptHandler&) = delete;
        InterruptHandler& operator=(InterruptHandler&& other) noexcept;
        ~InterruptHandler();

    public:
        virtual void Invoke() = 0;

        void Register(int32_t irq, InterruptPriority priority = InterruptPriority::normal);
        void Unregister();
        void ClearPending();

        bool Registered() const;
        int32_t Irq() const;
        InterruptPriority Priority() const;

    private:
        std::optional<int32_t> irq;
        InterruptPriority priority{ InterruptPriority::normal };
    };

    class InterruptTable
        : public infra::InterfaceConnector<InterruptTable>
    {
    public:
        template<std::size_t Size>
        using WithStorage = infra::WithStorage<InterruptTable, std::array<InterruptHandler*, Size>>;

        explicit InterruptTable(infra::MemoryRange<InterruptHandler*> table);

        void Invoke(int32_t irq);
        InterruptHandler* Handler(int32_t irq) const;

    private:
        friend class InterruptHandler;

        void RegisterHandler(int32_t irq, InterruptHandler& handler, InterruptPriority priority);
        void UnregisterHandler(int32_t irq, InterruptHandler& handler);
        void TakeOverHandler(int32_t irq, InterruptHandler& handler, const InterruptHandler& previous);

        std::size_t Index(int32_t irq) const;

    private:
        infra::MemoryRange<InterruptHandler*> table;
    };

    // Masks its interrupt on arrival and runs the callback from the event dispatcher,
    // re-enabling the interrupt once the callback has completed.
    class DispatchedInterruptHandler
        : public InterruptHandler
    {
    public:
        DispatchedInterruptHandler(int32_t irq, infra::Function<void()> onInvoke);
        DispatchedInterruptHandler(int32_t irq, InterruptPriority priority, infra::Function<void()> onInvoke);

        void Invoke() override;
        void SetInvoke(infra::Function<void()> onInvoke);

    private:
        static void InvokeScheduled(int32_t irq, DispatchedInterruptHandler& handler);

    private:
        infra::Function<void()> onInvoke;
        bool pending{ false };
    };

    // Runs the callback directly in interrupt context.
    class ImmediateInterruptHandler
        : public InterruptHandler
    {
    public:
        ImmediateInterruptHandler(int32_t irq, infra::Function<void()> onInvoke);
        ImmediateInterruptHandler(int32_t irq, InterruptPriority priority, infra::Function<void()> onInvoke);

        void Invoke() override;

    private:
        infra::Function<void()> onInvoke;
    };
}

#endif
