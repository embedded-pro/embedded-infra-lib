#include "services/peripheral/LedPattern.hpp"

namespace services
{
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
