#include "services/peripheral/LedPattern.hpp"

namespace services
{
    LedPattern LedPattern::Off()
    {
        return LedPattern{ Kind::off, 0, 0, 0, 0 };
    }

    LedPattern LedPattern::Steady()
    {
        return LedPattern{ Kind::steady, 0, 0, 0, 0 };
    }

    LedPattern LedPattern::Flashes(uint8_t count, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
    {
        return LedPattern{ Kind::flashes, count, onTicks, offTicks, pauseTicks };
    }

    LedPattern::LedPattern(Kind kind, uint8_t flashes, uint16_t onTicks, uint16_t offTicks, uint16_t pauseTicks)
        : kind{ kind }
        , flashes{ flashes }
        , onTicks{ onTicks }
        , offTicks{ offTicks }
        , pauseTicks{ pauseTicks }
    {}

    bool LedPattern::operator==(const LedPattern& other) const = default;

    uint32_t LedPattern::PeriodTicks() const
    {
        if (kind != Kind::flashes)
            return 1;
        return static_cast<uint32_t>(flashes) * (onTicks + offTicks) + pauseTicks;
    }

    bool LedPattern::LevelAt(uint32_t tick) const
    {
        if (kind != Kind::flashes)
            return kind == Kind::steady;

        const uint32_t flashLength = static_cast<uint32_t>(onTicks) + offTicks;
        const uint32_t position = tick % PeriodTicks();
        if (position >= flashes * flashLength)
            return false;
        return position % flashLength < onTicks;
    }

    LedPatternPlayer::LedPatternPlayer(hal::GpioPin& pin)
        : pin{ pin }
    {}

    void LedPatternPlayer::Update(const LedPattern& next)
    {
        if (!started || !(next == pattern))
        {
            pattern = next;
            tick = 0;
            started = true;
        }
        else
            ++tick;

        pin.Set(pattern.LevelAt(tick));
    }
}
