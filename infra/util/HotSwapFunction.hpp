#ifndef INFRA_HOT_SWAP_FUNCTION_HPP
#define INFRA_HOT_SWAP_FUNCTION_HPP

//  HotSwapFunction holds a callable that a thread replaces while an interrupt keeps invoking it.
//  Assigning to an infra::Function destroys the old callable (leaving the abort-on-execute sentinel) and then
//  copy-constructs the new one, so an interrupt that lands in between aborts. HotSwapFunction owns two
//  Function slots: Set fills the slot that is not in use and publishes it with a single atomic store of an index,
//  and operator() reads that index once, so the invoker always runs a complete old or new callable.
//
//  This is safe on a single core only, because the invoker preempts the setter: whenever Set or Clear run, the
//  invoker is not part way through a call. Set and Clear must therefore be called from a context the invoker
//  preempts (the main thread or a lower-priority interrupt), and a callable may replace itself at most once per call.

#include "infra/util/Function.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace infra
{
    template<class F, std::size_t ExtraSize = INFRA_DEFAULT_FUNCTION_EXTRA_SIZE>
    class HotSwapFunction;

    template<std::size_t ExtraSize, class... Args>
    class HotSwapFunction<void(Args...), ExtraSize>
    {
    public:
        using Callable = Function<void(Args...), ExtraSize>;

        HotSwapFunction() = default;
        HotSwapFunction(const HotSwapFunction& other) = delete;
        HotSwapFunction& operator=(const HotSwapFunction& other) = delete;
        ~HotSwapFunction() = default;

        void Set(const Callable& callable);
        void Clear();
        bool IsSet() const;

        void operator()(Args... args);

    private:
        static constexpr uint8_t none{ 2 };

        std::array<Callable, 2> slots;
        std::atomic<uint8_t> active{ none };
        uint8_t lastSet{ 1 };
    };

    //// Implementation ////

    template<std::size_t ExtraSize, class... Args>
    void HotSwapFunction<void(Args...), ExtraSize>::Set(const Callable& callable)
    {
        const uint8_t next = lastSet == 0 ? 1 : 0;

        slots[next] = callable;
        lastSet = next;
        active.store(next, std::memory_order_release);
    }

    template<std::size_t ExtraSize, class... Args>
    void HotSwapFunction<void(Args...), ExtraSize>::Clear()
    {
        active.store(none, std::memory_order_release);
    }

    template<std::size_t ExtraSize, class... Args>
    bool HotSwapFunction<void(Args...), ExtraSize>::IsSet() const
    {
        return active.load(std::memory_order_acquire) != none;
    }

    template<std::size_t ExtraSize, class... Args>
    void HotSwapFunction<void(Args...), ExtraSize>::operator()(Args... args)
    {
        const auto index = active.load(std::memory_order_relaxed);
        std::atomic_signal_fence(std::memory_order_acquire);

        if (index != none)
            slots[index](std::forward<Args>(args)...);
    }
}

#endif
