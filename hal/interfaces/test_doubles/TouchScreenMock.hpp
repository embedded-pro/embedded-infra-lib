#ifndef HAL_TOUCH_SCREEN_MOCK_HPP
#define HAL_TOUCH_SCREEN_MOCK_HPP

#include "hal/interfaces/TouchScreen.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class TouchScreenMock
        : public TouchScreen
    {
    public:
        MOCK_METHOD(TouchScreenSize, Size, (), (const, override));
        MOCK_METHOD(void, Start, (const infra::Function<void(Event event)>& onTouch), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif
