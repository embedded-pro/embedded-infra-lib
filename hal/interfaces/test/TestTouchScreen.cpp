#include "hal/interfaces/test_doubles/TouchScreenStub.hpp"
#include "gtest/gtest.h"
#include <vector>

namespace
{
    constexpr hal::TouchScreenSize screenSize{ 240, 320 };

    class TouchScreenTest
        : public testing::Test
    {
    public:
        void Start()
        {
            EXPECT_CALL(stub, Start(testing::_));

            touchScreen.Start([this](hal::TouchScreen::Event event)
                {
                    events.push_back(event);
                });
        }

        void Stop()
        {
            EXPECT_CALL(stub, Stop(testing::_));

            touchScreen.Stop([this]()
                {
                    ++stopped;
                });
        }

        testing::StrictMock<hal::TouchScreenStub> stub{ screenSize };
        hal::TouchScreen& touchScreen{ stub };
        std::vector<hal::TouchScreen::Event> events;
        int stopped{ 0 };
    };
}

TEST(TouchScreenValueTest, sizes_are_equal_when_width_and_height_are_equal)
{
    EXPECT_TRUE((hal::TouchScreenSize{ 1, 2 } == hal::TouchScreenSize{ 1, 2 }));
    EXPECT_FALSE((hal::TouchScreenSize{ 1, 2 } == hal::TouchScreenSize{ 3, 2 }));
    EXPECT_FALSE((hal::TouchScreenSize{ 1, 2 } == hal::TouchScreenSize{ 1, 3 }));
}

TEST(TouchScreenValueTest, points_are_equal_when_x_and_y_are_equal)
{
    EXPECT_TRUE((hal::TouchPoint{ 1, 2 } == hal::TouchPoint{ 1, 2 }));
    EXPECT_FALSE((hal::TouchPoint{ 1, 2 } == hal::TouchPoint{ 3, 2 }));
    EXPECT_FALSE((hal::TouchPoint{ 1, 2 } == hal::TouchPoint{ 1, 3 }));
}

TEST(TouchScreenValueTest, events_are_equal_when_phase_and_point_are_equal)
{
    using Event = hal::TouchScreen::Event;
    using Phase = hal::TouchScreen::Phase;

    EXPECT_TRUE((Event{ Phase::moved, { 1, 2 } } == Event{ Phase::moved, { 1, 2 } }));
    EXPECT_FALSE((Event{ Phase::moved, { 1, 2 } } == Event{ Phase::pressed, { 1, 2 } }));
    EXPECT_FALSE((Event{ Phase::moved, { 1, 2 } } == Event{ Phase::moved, { 1, 3 } }));
}

TEST_F(TouchScreenTest, Size_is_the_range_in_which_points_are_reported)
{
    EXPECT_EQ(screenSize, touchScreen.Size());
}

TEST_F(TouchScreenTest, no_event_is_received_before_a_touch)
{
    Start();

    EXPECT_TRUE(events.empty());
    EXPECT_TRUE(stub.Running());
}

TEST_F(TouchScreenTest, a_touch_is_pressed_moved_and_released)
{
    Start();

    stub.Press({ 10, 20 });
    stub.Move({ 11, 22 });
    stub.Move({ 12, 24 });
    stub.Release();

    using Event = hal::TouchScreen::Event;
    using Phase = hal::TouchScreen::Phase;
    ASSERT_EQ(std::size_t(4), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 10, 20 } }), events[0]);
    EXPECT_EQ((Event{ Phase::moved, { 11, 22 } }), events[1]);
    EXPECT_EQ((Event{ Phase::moved, { 12, 24 } }), events[2]);
    EXPECT_EQ((Event{ Phase::released, { 12, 24 } }), events[3]);
}

TEST_F(TouchScreenTest, a_touch_without_movement_is_pressed_and_released_at_the_same_point)
{
    Start();

    stub.Press({ 5, 6 });
    stub.Release();

    using Event = hal::TouchScreen::Event;
    using Phase = hal::TouchScreen::Phase;
    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 5, 6 } }), events[0]);
    EXPECT_EQ((Event{ Phase::released, { 5, 6 } }), events[1]);
}

TEST_F(TouchScreenTest, points_on_the_last_column_and_row_are_reported)
{
    Start();

    stub.Press({ screenSize.width - 1, screenSize.height - 1 });

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((hal::TouchPoint{ 239, 319 }), events[0].point);
}

TEST_F(TouchScreenTest, a_second_touch_follows_the_first)
{
    Start();

    stub.Press({ 1, 1 });
    stub.Release();
    stub.Press({ 2, 2 });

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ(hal::TouchScreen::Phase::pressed, events[2].phase);
    EXPECT_EQ((hal::TouchPoint{ 2, 2 }), events[2].point);
}

TEST_F(TouchScreenTest, no_event_is_received_after_Stop_even_before_it_completes)
{
    Start();
    Stop();

    stub.Press({ 1, 1 });

    EXPECT_TRUE(events.empty());
    EXPECT_FALSE(stub.Running());
}

TEST_F(TouchScreenTest, Stop_completes_later_and_not_from_within_Stop)
{
    Start();
    Stop();

    EXPECT_EQ(0, stopped);
    EXPECT_TRUE(stub.StopPending());

    stub.CompleteStop();

    EXPECT_EQ(1, stopped);
    EXPECT_FALSE(stub.StopPending());
}

TEST_F(TouchScreenTest, Stop_during_a_touch_ends_it_without_a_released)
{
    Start();
    stub.Press({ 1, 1 });
    Stop();

    stub.Release();

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_FALSE(stub.Touching());
}

TEST_F(TouchScreenTest, consumer_may_stop_from_within_an_event)
{
    EXPECT_CALL(stub, Start(testing::_));
    EXPECT_CALL(stub, Stop(testing::_));

    touchScreen.Start([this](hal::TouchScreen::Event event)
        {
            events.push_back(event);
            touchScreen.Stop([this]()
                {
                    ++stopped;
                });
        });

    stub.Press({ 1, 1 });
    stub.Press({ 2, 2 });
    stub.CompleteStop();

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(1, stopped);
}

TEST_F(TouchScreenTest, restarting_after_the_stop_completed_registers_a_new_callback)
{
    Start();
    Stop();
    stub.CompleteStop();

    int restartedEvents{ 0 };
    EXPECT_CALL(stub, Start(testing::_));
    touchScreen.Start([&](hal::TouchScreen::Event)
        {
            ++restartedEvents;
        });

    stub.Press({ 1, 1 });

    EXPECT_EQ(1, restartedEvents);
    EXPECT_TRUE(events.empty());
}

TEST_F(TouchScreenTest, the_stop_can_be_followed_by_a_start_from_within_its_completion)
{
    Start();
    EXPECT_CALL(stub, Stop(testing::_));
    EXPECT_CALL(stub, Start(testing::_));
    int restartedEvents{ 0 };

    touchScreen.Stop([&]()
        {
            touchScreen.Start([&](hal::TouchScreen::Event)
                {
                    ++restartedEvents;
                });
        });
    stub.CompleteStop();
    stub.Press({ 1, 1 });

    EXPECT_EQ(1, restartedEvents);
}

TEST_F(TouchScreenTest, a_start_before_the_stop_completed_is_not_allowed)
{
    Start();
    Stop();

    EXPECT_DEATH(Start(), "");
}

TEST_F(TouchScreenTest, a_second_stop_before_the_first_completed_is_not_allowed)
{
    Start();
    Stop();

    EXPECT_DEATH(Stop(), "");
}
