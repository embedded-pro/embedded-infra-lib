#include "hal/interfaces/TouchScreen.hpp"

namespace hal
{
    bool TouchScreenSize::operator==(const TouchScreenSize& other) const
    {
        return width == other.width && height == other.height;
    }

    bool TouchPoint::operator==(const TouchPoint& other) const
    {
        return x == other.x && y == other.y;
    }

    bool TouchScreen::Event::operator==(const Event& other) const
    {
        return phase == other.phase && point == other.point;
    }
}
