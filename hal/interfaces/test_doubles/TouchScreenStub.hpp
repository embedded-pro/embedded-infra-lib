#ifndef HAL_TOUCH_SCREEN_STUB_HPP
#define HAL_TOUCH_SCREEN_STUB_HPP

#include "hal/interfaces/test_doubles/TouchScreenMock.hpp"

namespace hal
{
    class TouchScreenStub
        : public TouchScreenMock
    {
    public:
        explicit TouchScreenStub(TouchScreenSize size);

        TouchScreenSize Size() const override;

        void Press(TouchPoint point);
        void Move(TouchPoint point);
        void Release();

        bool Running() const;
        bool Touching() const;

    private:
        void Deliver(Phase phase, TouchPoint point);

    private:
        TouchScreenSize size;
        bool running{ false };
        bool touching{ false };
        TouchPoint lastPoint{};
        infra::Function<void(Event)> onTouch;
    };
}

#endif // HAL_TOUCH_SCREEN_STUB_HPP
