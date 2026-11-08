#ifndef SERVICES_LED_PATTERN_HPP
#define SERVICES_LED_PATTERN_HPP

#include "hal/interfaces/Gpio.hpp"
#include <cstdint>

namespace services
{
    class LedPattern
    {
    public:
        static constexpr LedPattern Off()
        {
            return LedPattern{ Kind::off, 0, 0, 0, 0 };
        }

        static constexpr LedPattern Steady()
        {
            return LedPattern{ Kind::steady, 0, 0, 0, 0 };
        }

        static constexpr LedPattern Flashes(uint8_t count, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
        {
            return LedPattern{ Kind::flashes, count, onTicks, offTicks, pauseTicks };
        }

        bool LevelAt(uint32_t tick) const;
        uint32_t PeriodTicks() const;

        bool operator==(const LedPattern&) const = default;

    private:
        enum class Kind : uint8_t
        {
            off,
            steady,
            flashes
        };

        constexpr LedPattern(Kind kind, uint8_t flashes, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
            : kind{ kind }
            , flashes{ flashes }
            , onTicks{ onTicks }
            , offTicks{ offTicks }
            , pauseTicks{ pauseTicks }
        {}

        Kind kind;
        uint8_t flashes;
        uint16_t onTicks;
        uint16_t offTicks;
        uint16_t pauseTicks;
    };

    class LedPatternPlayer
    {
    public:
        explicit LedPatternPlayer(hal::GpioPin& pin);

        void Update(const LedPattern& next);

    private:
        hal::OutputPin pin;
        LedPattern pattern{ LedPattern::Off() };
        uint32_t tick{ 0 };
        bool started{ false };
    };
}

#endif
