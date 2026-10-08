#include "hal/interfaces/test_doubles/TouchScreenStub.hpp"
#include "infra/util/ReallyAssert.hpp"

namespace hal
{
    TouchScreenStub::TouchScreenStub(TouchScreenSize size)
        : size(size)
    {
        ON_CALL(*this, Start).WillByDefault([this](const infra::Function<void(Event)>& onTouchCallback)
            {
                really_assert(!running);
                running = true;
                touching = false;
                onTouch = onTouchCallback;
            });

        ON_CALL(*this, Stop).WillByDefault([this]()
            {
                running = false;
                touching = false;
                onTouch = nullptr;
            });
    }

    TouchScreenSize TouchScreenStub::Size() const
    {
        return size;
    }

    void TouchScreenStub::Press(TouchPoint point)
    {
        if (!running)
            return;

        really_assert(!touching);
        touching = true;
        Deliver(Phase::pressed, point);
    }

    void TouchScreenStub::Move(TouchPoint point)
    {
        if (!running)
            return;

        really_assert(touching);
        really_assert(!(point == lastPoint));
        Deliver(Phase::moved, point);
    }

    void TouchScreenStub::Release()
    {
        if (!running)
            return;

        really_assert(touching);
        touching = false;
        Deliver(Phase::released, lastPoint);
    }

    bool TouchScreenStub::Running() const
    {
        return running;
    }

    bool TouchScreenStub::Touching() const
    {
        return touching;
    }

    void TouchScreenStub::Deliver(Phase phase, TouchPoint point)
    {
        really_assert(point.x < size.width && point.y < size.height);
        lastPoint = point;

        auto localOnTouch = onTouch;
        localOnTouch(Event{ phase, point });
    }
}
