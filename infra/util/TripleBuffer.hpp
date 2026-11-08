#ifndef INFRA_TRIPLE_BUFFER_HPP
#define INFRA_TRIPLE_BUFFER_HPP

#include <array>
#include <atomic>
#include <cstdint>

namespace infra
{
    namespace detail
    {
        constexpr uint8_t TripleBufferNextFreeSlot(uint8_t slotCount, uint8_t ready, uint8_t held)
        {
            if (ready != held)
                return static_cast<uint8_t>(slotCount * (slotCount - 1u) / 2u - ready - held);

            return static_cast<uint8_t>(ready == slotCount - 1u ? 0u : ready + 1u);
        }
    }

    template<class T>
    class TripleBuffer
    {
    public:
        void Publish(const T& value)
        {
            slots[writeSlot] = value;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            ready = writeSlot;
            writeSlot = detail::TripleBufferNextFreeSlot(slotCount, ready, held);
        }

        const T& Acquire()
        {
            held = ready;
            std::atomic_signal_fence(std::memory_order_seq_cst);
            return slots[held];
        }

    private:
        static constexpr uint8_t slotCount{ 3 };

        std::array<T, slotCount> slots{};
        uint8_t writeSlot{ 1 };
        volatile uint8_t ready{ 0 };
        volatile uint8_t held{ 0 };
    };
}

#endif
