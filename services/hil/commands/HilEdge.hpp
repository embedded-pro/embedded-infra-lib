#ifndef SERVICES_HIL_EDGE_HPP
#define SERVICES_HIL_EDGE_HPP

#include "hal/interfaces/Gpio.hpp"
#include "services/hil/HilArguments.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <optional>

namespace services
{
    enum class HilEdge : uint8_t
    {
        rising,
        falling,
        both,
        off,
    };

    inline constexpr std::array<HilChoice<HilEdge>, 4> hilEdges{ {
        { "rising", HilEdge::rising },
        { "falling", HilEdge::falling },
        { "both", HilEdge::both },
        { "off", HilEdge::off },
    } };

    constexpr std::optional<hal::InterruptTrigger> ToTrigger(HilEdge edge)
    {
        switch (edge)
        {
            case HilEdge::rising:
                return hal::InterruptTrigger::risingEdge;
            case HilEdge::falling:
                return hal::InterruptTrigger::fallingEdge;
            case HilEdge::both:
                return hal::InterruptTrigger::bothEdges;
            default:
                return std::nullopt;
        }
    }

    class HilEdgeCounter
    {
    public:
        void Increment();
        uint32_t Read(bool clear);
        void Reset();

    private:
        std::atomic<uint32_t> count{ 0 };
    };

    inline void HilEdgeCounter::Increment()
    {
        count.fetch_add(1);
    }

    inline uint32_t HilEdgeCounter::Read(bool clear)
    {
        return clear ? count.exchange(0) : count.load();
    }

    inline void HilEdgeCounter::Reset()
    {
        count = 0;
    }
}

#endif
