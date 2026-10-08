#ifndef HAL_TOUCH_SCREEN_HPP
#define HAL_TOUCH_SCREEN_HPP

#include "infra/util/Function.hpp"
#include <cstdint>

namespace hal
{
    struct TouchScreenSize
    {
        uint16_t width;
        uint16_t height;

        bool operator==(const TouchScreenSize& other) const;
    };

    struct TouchPoint
    {
        uint16_t x;
        uint16_t y;

        bool operator==(const TouchPoint& other) const;
    };

    class TouchScreen
    {
    public:
        enum class Phase : uint8_t
        {
            pressed,
            moved,
            released
        };

        struct Event
        {
            Phase phase;
            TouchPoint point;

            bool operator==(const Event& other) const;
        };

        virtual TouchScreenSize Size() const = 0;
        virtual void Start(const infra::Function<void(Event event)>& onTouch) = 0;
        virtual void Stop() = 0;

    protected:
        TouchScreen() = default;
        TouchScreen(const TouchScreen& other) = delete;
        TouchScreen& operator=(const TouchScreen& other) = delete;
        ~TouchScreen() = default;
    };
}

#endif
