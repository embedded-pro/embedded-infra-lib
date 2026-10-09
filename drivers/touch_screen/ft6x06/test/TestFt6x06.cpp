#include "drivers/touch_screen/ft6x06/Ft6x06.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;
    using Event = hal::TouchScreen::Event;
    using Phase = hal::TouchScreen::Phase;
    using InitializationResult = drivers::Ft6x06::InitializationResult;

    constexpr uint8_t touchStatusRegister = 0x02;
    constexpr uint8_t chipIdRegister = 0xa3;
    constexpr uint8_t vendorIdRegister = 0xa8;

    std::chrono::milliseconds Milliseconds(int count)
    {
        return std::chrono::milliseconds(count);
    }

    Bytes Sample(uint8_t touches, uint16_t x, uint16_t y)
    {
        constexpr uint8_t contactEventFlag = 0x80;
        constexpr uint8_t firstTouchId = 0x10;

        return { touches, static_cast<uint8_t>(contactEventFlag | (x >> 8)), static_cast<uint8_t>(x & 0xff), static_cast<uint8_t>(firstTouchId | (y >> 8)), static_cast<uint8_t>(y & 0xff) };
    }

    Bytes OneTouch(uint16_t x, uint16_t y)
    {
        return Sample(1, x, y);
    }

    Bytes NoTouch()
    {
        return { 0, 0, 0, 0, 0 };
    }

    class Ft6x06Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        void ExpectIdentification(uint8_t vendorId = 0x11, uint8_t chipId = 0x64)
        {
            EXPECT_CALL(bus, ReadRegisterMock(vendorIdRegister, 1)).WillOnce(testing::Return(Bytes{ vendorId }));
            EXPECT_CALL(bus, ReadRegisterMock(chipIdRegister, 1)).WillOnce(testing::Return(Bytes{ chipId }));
        }

        void ExpectSample(const Bytes& sample)
        {
            EXPECT_CALL(bus, ReadRegisterMock(touchStatusRegister, 5)).WillOnce(testing::Return(sample));
        }

        void Create(const drivers::Ft6x06::Config& config = {})
        {
            touchScreen.emplace(bus, config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });
        }

        void CreateAndInitialize(const drivers::Ft6x06::Config& config = {})
        {
            ExpectIdentification();
            Create(config);
            ExecuteAllActions();
        }

        void Start()
        {
            touchScreen->Start([this](Event event)
                {
                    events.push_back(event);
                });
        }

        void Stop()
        {
            touchScreen->Stop([this]()
                {
                    ++stopped;
                });
        }

        void StartWithoutTouch()
        {
            ExpectSample(NoTouch());
            Start();
            ExecuteAllActions();
        }

        void Poll(const Bytes& sample)
        {
            ExpectSample(sample);
            ForwardTime(Milliseconds(20));
        }

        void StartAndPressAt(uint16_t x, uint16_t y)
        {
            ExpectSample(OneTouch(x, y));
            Start();
            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        std::optional<drivers::Ft6x06> touchScreen;
        std::optional<InitializationResult> initializationResult;
        std::vector<Event> events;
        int stopped{ 0 };
    };
}

TEST_F(Ft6x06Test, the_vendor_id_and_the_chip_id_are_read_and_the_device_is_reported_as_initialized)
{
    testing::InSequence sequence;
    ExpectIdentification(0x11, 0x64);

    Create();
    EXPECT_FALSE(initializationResult);
    ExecuteAllActions();

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
    EXPECT_EQ(0x11, touchScreen->VendorId());
    EXPECT_EQ(0x64, touchScreen->ChipId());
}

TEST_F(Ft6x06Test, a_device_that_answers_with_vendor_id_0x00_is_identified_again_after_100_milliseconds)
{
    testing::InSequence sequence;
    ExpectIdentification(0x00, 0x00);
    Create();
    ExecuteAllActions();
    ForwardTime(Milliseconds(99));
    EXPECT_FALSE(initializationResult);

    ExpectIdentification(0x11, 0x64);
    ForwardTime(Milliseconds(1));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
    EXPECT_EQ(0x11, touchScreen->VendorId());
}

TEST_F(Ft6x06Test, a_device_that_answers_with_vendor_id_0xff_is_identified_again)
{
    testing::InSequence sequence;
    ExpectIdentification(0xff, 0xff);
    Create();
    ExecuteAllActions();

    ExpectIdentification(0x11, 0x64);
    ForwardTime(Milliseconds(100));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Ft6x06Test, a_device_that_never_answers_is_reported_as_not_found_after_30_attempts)
{
    EXPECT_CALL(bus, ReadRegisterMock(vendorIdRegister, 1)).Times(30).WillRepeatedly(testing::Return(Bytes{ 0x00 }));
    EXPECT_CALL(bus, ReadRegisterMock(chipIdRegister, 1)).Times(30).WillRepeatedly(testing::Return(Bytes{ 0x00 }));

    Create();
    ExecuteAllActions();
    ForwardTime(Milliseconds(2899));
    EXPECT_FALSE(initializationResult);

    ForwardTime(Milliseconds(1));
    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);

    ForwardTime(std::chrono::seconds(10));
}

TEST_F(Ft6x06Test, the_number_of_attempts_and_the_retry_interval_are_configurable)
{
    drivers::Ft6x06::Config config;
    config.identificationAttempts = 2;
    config.identificationRetryInterval = Milliseconds(250);

    EXPECT_CALL(bus, ReadRegisterMock(vendorIdRegister, 1)).Times(2).WillRepeatedly(testing::Return(Bytes{ 0x00 }));
    EXPECT_CALL(bus, ReadRegisterMock(chipIdRegister, 1)).Times(2).WillRepeatedly(testing::Return(Bytes{ 0x00 }));

    Create(config);
    ExecuteAllActions();
    ForwardTime(Milliseconds(249));
    EXPECT_FALSE(initializationResult);

    ForwardTime(Milliseconds(1));
    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);

    ForwardTime(std::chrono::seconds(10));
}

TEST_F(Ft6x06Test, a_device_that_was_not_found_cannot_be_started)
{
    drivers::Ft6x06::Config config;
    config.identificationAttempts = 1;
    ExpectIdentification(0x00, 0x00);
    Create(config);
    ExecuteAllActions();

    EXPECT_DEATH(Start(), "");
}

TEST_F(Ft6x06Test, the_device_cannot_be_started_before_it_is_initialized)
{
    ExpectIdentification();
    Create();

    EXPECT_DEATH(Start(), "");

    ExecuteAllActions();
}

TEST_F(Ft6x06Test, a_poll_interval_of_zero_is_not_accepted)
{
    drivers::Ft6x06::Config config;
    config.pollInterval = infra::Duration::zero();

    EXPECT_DEATH(Create(config), "");
}

TEST_F(Ft6x06Test, a_retry_interval_of_zero_is_not_accepted)
{
    drivers::Ft6x06::Config config;
    config.identificationRetryInterval = infra::Duration::zero();

    EXPECT_DEATH(Create(config), "");
}

TEST_F(Ft6x06Test, zero_identification_attempts_are_not_accepted)
{
    drivers::Ft6x06::Config config;
    config.identificationAttempts = 0;

    EXPECT_DEATH(Create(config), "");
}

TEST_F(Ft6x06Test, an_empty_size_is_not_accepted)
{
    drivers::Ft6x06::Config config;
    config.size = hal::TouchScreenSize{ 0, 800 };
    EXPECT_DEATH(Create(config), "");

    config.size = hal::TouchScreenSize{ 480, 0 };
    EXPECT_DEATH(Create(config), "");
}

TEST_F(Ft6x06Test, the_size_is_480_by_800_unless_configured_otherwise)
{
    CreateAndInitialize();
    EXPECT_EQ((hal::TouchScreenSize{ 480, 800 }), touchScreen->Size());

    touchScreen.reset();
    drivers::Ft6x06::Config config;
    config.size = hal::TouchScreenSize{ 240, 320 };
    CreateAndInitialize(config);
    EXPECT_EQ((hal::TouchScreenSize{ 240, 320 }), touchScreen->Size());
}

TEST_F(Ft6x06Test, the_points_are_in_the_orientation_of_the_panel_unless_configured_otherwise)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    EXPECT_EQ((hal::TouchScreenSize{ 480, 800 }), touchScreen->Size());
    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 200 } }), events[0]);
}

TEST_F(Ft6x06Test, swapping_the_axes_exchanges_x_and_y_and_the_width_and_the_height)
{
    drivers::Ft6x06::Config config;
    config.orientation.swapAxes = true;
    CreateAndInitialize(config);
    StartAndPressAt(100, 200);

    EXPECT_EQ((hal::TouchScreenSize{ 800, 480 }), touchScreen->Size());
    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 200, 100 } }), events[0]);
}

TEST_F(Ft6x06Test, mirroring_x_runs_x_from_the_last_column_to_the_first)
{
    drivers::Ft6x06::Config config;
    config.orientation.mirrorX = true;
    CreateAndInitialize(config);
    StartAndPressAt(100, 200);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 379, 200 } }), events[0]);
}

TEST_F(Ft6x06Test, mirroring_y_runs_y_from_the_last_row_to_the_first)
{
    drivers::Ft6x06::Config config;
    config.orientation.mirrorY = true;
    CreateAndInitialize(config);
    StartAndPressAt(100, 200);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 599 } }), events[0]);
}

TEST_F(Ft6x06Test, the_axes_are_swapped_before_they_are_mirrored)
{
    drivers::Ft6x06::Config config;
    config.orientation = drivers::Ft6x06::Orientation{ true, true, false };
    CreateAndInitialize(config);
    StartAndPressAt(100, 200);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 599, 100 } }), events[0]);
}

TEST_F(Ft6x06Test, a_portrait_panel_driven_in_landscape_swaps_the_axes_and_mirrors_y)
{
    drivers::Ft6x06::Config config;
    config.orientation = drivers::Ft6x06::Orientation{ true, false, true };
    CreateAndInitialize(config);

    ExpectSample(OneTouch(0, 0));
    Start();
    ExecuteAllActions();
    Poll(OneTouch(479, 799));

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 0, 479 } }), events[0]);
    EXPECT_EQ((Event{ Phase::moved, { 799, 0 } }), events[1]);
}

TEST_F(Ft6x06Test, a_position_beyond_the_size_is_limited_before_it_is_oriented)
{
    drivers::Ft6x06::Config config;
    config.orientation = drivers::Ft6x06::Orientation{ true, false, true };
    CreateAndInitialize(config);
    StartAndPressAt(0xfff, 0xfff);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 799, 0 } }), events[0]);
}

TEST_F(Ft6x06Test, a_release_repeats_the_oriented_last_position)
{
    drivers::Ft6x06::Config config;
    config.orientation = drivers::Ft6x06::Orientation{ true, false, true };
    CreateAndInitialize(config);
    StartAndPressAt(100, 200);

    Poll(NoTouch());

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::released, { 200, 379 } }), events[1]);
}

TEST_F(Ft6x06Test, Start_reads_the_touch_status_and_a_status_without_a_touch_reports_nothing)
{
    CreateAndInitialize();

    ExpectSample(NoTouch());
    Start();
    ExecuteAllActions();

    EXPECT_TRUE(events.empty());
}

TEST_F(Ft6x06Test, a_touch_is_reported_from_the_event_dispatcher_and_not_from_within_Start)
{
    CreateAndInitialize();

    ExpectSample(OneTouch(10, 20));
    Start();
    EXPECT_TRUE(events.empty());

    ExecuteAllActions();
    EXPECT_EQ(std::size_t(1), events.size());
}

TEST_F(Ft6x06Test, the_touch_status_is_polled_every_20_milliseconds)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ForwardTime(Milliseconds(19));
    Poll(NoTouch());
    ForwardTime(Milliseconds(0));
    Poll(NoTouch());
}

TEST_F(Ft6x06Test, the_poll_interval_is_configurable)
{
    drivers::Ft6x06::Config config;
    config.pollInterval = Milliseconds(5);
    CreateAndInitialize(config);
    StartWithoutTouch();

    ForwardTime(Milliseconds(4));
    ExpectSample(NoTouch());
    ForwardTime(Milliseconds(1));
    ExpectSample(NoTouch());
    ForwardTime(Milliseconds(5));
}

TEST_F(Ft6x06Test, the_first_touch_is_reported_as_pressed_at_its_position)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 200 } }), events[0]);
}

TEST_F(Ft6x06Test, the_position_is_12_bits_for_x_and_for_y)
{
    CreateAndInitialize();
    StartAndPressAt(0x123, 0x2ab);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 0x123, 0x2ab } }), events[0]);
}

TEST_F(Ft6x06Test, the_event_flag_and_the_touch_id_in_the_high_bytes_are_not_part_of_the_position)
{
    CreateAndInitialize();

    ExpectSample(Bytes{ 1, 0xc1, 0x23, 0xf2, 0xab });
    Start();
    ExecuteAllActions();

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 0x123, 0x2ab } }), events[0]);
}

TEST_F(Ft6x06Test, a_position_beyond_the_size_is_limited_to_the_last_coordinate)
{
    CreateAndInitialize();
    StartAndPressAt(0xfff, 0xfff);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 479, 799 } }), events[0]);
}

TEST_F(Ft6x06Test, a_position_that_did_not_change_is_not_reported_again)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Poll(OneTouch(100, 200));
    Poll(OneTouch(100, 200));

    EXPECT_EQ(std::size_t(1), events.size());
}

TEST_F(Ft6x06Test, a_change_in_only_one_axis_is_a_move)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Poll(OneTouch(101, 200));
    Poll(OneTouch(101, 199));

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::moved, { 101, 200 } }), events[1]);
    EXPECT_EQ((Event{ Phase::moved, { 101, 199 } }), events[2]);
}

TEST_F(Ft6x06Test, a_lifted_finger_is_reported_as_released_at_the_last_position)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Poll(OneTouch(110, 210));
    Poll(NoTouch());

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::released, { 110, 210 } }), events[2]);
}

TEST_F(Ft6x06Test, polling_continues_after_the_release_and_nothing_more_is_reported_for_an_untouched_panel)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    Poll(NoTouch());

    Poll(NoTouch());
    Poll(NoTouch());

    EXPECT_EQ(std::size_t(2), events.size());
}

TEST_F(Ft6x06Test, a_second_touch_is_pressed_again_at_its_own_position)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    Poll(NoTouch());

    Poll(OneTouch(300, 400));

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 300, 400 } }), events[2]);
}

TEST_F(Ft6x06Test, with_two_fingers_the_first_touch_point_is_reported)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Poll(Sample(2, 105, 205));

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::moved, { 105, 205 } }), events[1]);
}

TEST_F(Ft6x06Test, a_touch_count_above_two_is_not_a_touch_and_does_not_end_one)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Poll(Sample(0x0f, 0, 0));
    Poll(OneTouch(100, 200));

    EXPECT_EQ(std::size_t(1), events.size());
}

TEST_F(Ft6x06Test, a_touch_count_above_two_without_a_touch_in_progress_reports_nothing)
{
    CreateAndInitialize();
    StartWithoutTouch();

    Poll(Sample(0x0f, 10, 20));

    EXPECT_TRUE(events.empty());
}

TEST_F(Ft6x06Test, the_upper_bits_of_the_touch_status_are_not_part_of_the_touch_count)
{
    CreateAndInitialize();
    StartWithoutTouch();

    Poll(Sample(0xf1, 10, 20));

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 10, 20 } }), events[0]);
}

TEST_F(Ft6x06Test, Stop_discards_a_touch_in_progress_without_a_released)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Stop();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, Stop_is_reported_from_the_event_dispatcher_and_not_from_within_Stop)
{
    CreateAndInitialize();
    StartWithoutTouch();

    Stop();
    EXPECT_EQ(0, stopped);

    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, Stop_ends_the_polling)
{
    CreateAndInitialize();
    StartWithoutTouch();

    Stop();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, Start_after_the_stop_is_reported_reports_a_touch_that_is_still_in_progress_as_pressed_again)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    Stop();
    ExecuteAllActions();

    StartAndPressAt(100, 200);

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 200 } }), events[1]);
}

TEST_F(Ft6x06Test, Start_after_the_stop_is_reported_uses_the_new_callback_only)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    Stop();
    ExecuteAllActions();

    std::vector<Event> newEvents;
    ExpectSample(NoTouch());
    touchScreen->Start([&newEvents](Event event)
        {
            newEvents.push_back(event);
        });
    ExecuteAllActions();
    Poll(OneTouch(1, 2));

    EXPECT_EQ(std::size_t(1), events.size());
    ASSERT_EQ(std::size_t(1), newEvents.size());
    EXPECT_EQ((Event{ Phase::pressed, { 1, 2 } }), newEvents[0]);
}

TEST_F(Ft6x06Test, Stop_before_Start_is_reported_and_leaves_the_device_alone)
{
    CreateAndInitialize();

    Stop();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, Start_twice_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();

    EXPECT_DEATH(Start(), "");
}

TEST_F(Ft6x06Test, Start_before_the_stop_is_reported_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(Start(), "");

    ExecuteAllActions();
}

TEST_F(Ft6x06Test, a_second_Stop_before_the_first_is_reported_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(Stop(), "");

    ExecuteAllActions();
}

TEST_F(Ft6x06Test, the_driver_cannot_be_destroyed_before_the_stop_is_reported)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(touchScreen.reset(), "");

    ExecuteAllActions();
    touchScreen.reset();
}

TEST_F(Ft6x06Test, the_driver_cannot_be_destroyed_while_a_bus_transaction_is_outstanding)
{
    CreateAndInitialize();

    bus.completeAutomatically = false;
    ExpectSample(NoTouch());
    Start();

    EXPECT_DEATH(touchScreen.reset(), "");

    bus.completeAutomatically = true;
    bus.CompletePending();
    ExecuteAllActions();
}

TEST_F(Ft6x06Test, Stop_during_the_status_read_discards_what_was_read)
{
    CreateAndInitialize();

    EXPECT_CALL(bus, ReadRegisterMock(touchStatusRegister, 5)).WillOnce(testing::Invoke([this](uint8_t, std::size_t)
        {
            Stop();
            return OneTouch(1, 2);
        }));
    Start();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, Stop_while_the_status_read_is_in_flight_is_reported_when_the_bus_transaction_has_completed)
{
    CreateAndInitialize();

    bus.completeAutomatically = false;
    ExpectSample(OneTouch(1, 2));
    Start();
    Stop();
    ExecuteAllActions();
    EXPECT_EQ(0, stopped);

    bus.completeAutomatically = true;
    bus.CompletePending();
    ExecuteAllActions();

    EXPECT_EQ(1, stopped);
    EXPECT_TRUE(events.empty());
    touchScreen.reset();
}

TEST_F(Ft6x06Test, Stop_during_the_identification_is_reported_when_the_initialization_has_finished)
{
    ExpectIdentification();
    Create();
    Stop();
    EXPECT_EQ(0, stopped);

    ExecuteAllActions();

    EXPECT_EQ(1, stopped);
    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Ft6x06Test, Stop_during_a_failed_identification_is_reported_after_the_last_attempt)
{
    drivers::Ft6x06::Config config;
    config.identificationAttempts = 2;
    EXPECT_CALL(bus, ReadRegisterMock(vendorIdRegister, 1)).Times(2).WillRepeatedly(testing::Return(Bytes{ 0x00 }));
    EXPECT_CALL(bus, ReadRegisterMock(chipIdRegister, 1)).Times(2).WillRepeatedly(testing::Return(Bytes{ 0x00 }));
    Create(config);
    ExecuteAllActions();
    Stop();
    ExecuteAllActions();
    EXPECT_EQ(0, stopped);

    ForwardTime(Milliseconds(100));

    EXPECT_EQ(1, stopped);
    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Ft6x06Test, the_consumer_may_stop_from_within_an_event)
{
    CreateAndInitialize();

    ExpectSample(OneTouch(1, 2));
    touchScreen->Start([this](Event event)
        {
            events.push_back(event);
            Stop();
        });
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(1, stopped);
}

TEST_F(Ft6x06Test, the_consumer_may_start_again_from_within_the_stopped_callback)
{
    CreateAndInitialize();

    int restartedEvents{ 0 };
    ExpectSample(OneTouch(1, 2));
    touchScreen->Start([&](Event event)
        {
            events.push_back(event);
            touchScreen->Stop([&]()
                {
                    ExpectSample(OneTouch(3, 4));
                    touchScreen->Start([&](Event)
                        {
                            ++restartedEvents;
                        });
                });
        });
    ExecuteAllActions();
    EXPECT_EQ(1, restartedEvents);

    ExpectSample(OneTouch(5, 6));
    ForwardTime(Milliseconds(20));

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(2, restartedEvents);
}
