#include "drivers/touch_screen/stmpe811/Stmpe811.hpp"
#include "hal/interfaces/test_doubles/GpioStub.hpp"
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
    using InitializationResult = drivers::Stmpe811::InitializationResult;

    constexpr uint8_t interruptStatusRegister = 0x0b;
    constexpr uint8_t touchControlRegister = 0x40;
    constexpr uint8_t fifoStatusRegister = 0x4b;
    constexpr uint8_t fifoSizeRegister = 0x4c;
    constexpr uint8_t touchDataRegister = 0xd7;

    constexpr uint8_t touching = 0x80;
    constexpr uint8_t notTouching = 0x00;

    std::chrono::milliseconds Milliseconds(int count)
    {
        return std::chrono::milliseconds(count);
    }

    Bytes Sample(uint16_t x, uint16_t y, uint8_t pressure = 0x55)
    {
        return { static_cast<uint8_t>(x >> 4), static_cast<uint8_t>(((x & 0x0f) << 4) | (y >> 8)), static_cast<uint8_t>(y & 0xff), pressure };
    }

    class Stmpe811Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Stmpe811Test()
        {
            interruptPin.SetStubState(true);
        }

        void ExpectWrite(uint8_t address, uint8_t value)
        {
            EXPECT_CALL(bus, WriteRegisterMock(address, Bytes{ value }));
        }

        void ExpectReset()
        {
            ExpectWrite(0x03, 0x02);
        }

        void ExpectIdentification(Bytes chipId = { 0x08, 0x11 })
        {
            ExpectWrite(0x03, 0x00);
            EXPECT_CALL(bus, ReadRegisterMock(0x00, 2)).WillOnce(testing::Return(chipId));
        }

        void ExpectConfiguration()
        {
            ExpectWrite(0x04, 0x08);
            ExpectWrite(0x17, 0x0f);
            ExpectWrite(0x20, 0x48);
            ExpectWrite(0x21, 0x01);
            ExpectWrite(0x41, 0x9a);
            ExpectWrite(0x4a, 0x01);
            ExpectWrite(fifoStatusRegister, 0x01);
            ExpectWrite(fifoStatusRegister, 0x00);
            ExpectWrite(0x56, 0x01);
            ExpectWrite(0x58, 0x01);
            ExpectWrite(touchControlRegister, 0x01);
            ExpectWrite(0x0a, 0x03);
            ExpectWrite(interruptStatusRegister, 0xff);
            ExpectWrite(0x09, 0x01);
        }

        void ExpectInterruptStatusCleared()
        {
            ExpectWrite(interruptStatusRegister, 0xff);
        }

        void ExpectStatus(uint8_t touchControl, uint8_t fifoSize)
        {
            EXPECT_CALL(bus, ReadRegisterMock(touchControlRegister, 1)).WillOnce(testing::Return(Bytes{ touchControl }));
            EXPECT_CALL(bus, ReadRegisterMock(fifoSizeRegister, 1)).WillOnce(testing::Return(Bytes{ fifoSize }));
        }

        void ExpectPollWithInterrupt(uint8_t touchControl, uint8_t fifoSize)
        {
            ExpectInterruptStatusCleared();
            ExpectStatus(touchControl, fifoSize);
        }

        void ExpectFifoFlush()
        {
            ExpectWrite(fifoStatusRegister, 0x01);
            ExpectWrite(fifoStatusRegister, 0x00);
        }

        void ExpectFetch(uint16_t x, uint16_t y)
        {
            EXPECT_CALL(bus, ReadRegisterMock(touchDataRegister, 4)).WillOnce(testing::Return(Sample(x, y)));
            ExpectFifoFlush();
        }

        void Create(const drivers::Stmpe811::Config& config = {})
        {
            Create(interruptPin, config);
        }

        void Create(hal::GpioPin& pin, const drivers::Stmpe811::Config& config = {})
        {
            touchScreen.emplace(bus, pin, config, [this](InitializationResult result)
                {
                    initializationResult = result;
                });
        }

        void CreateAndInitialize(hal::GpioPin& pin, const drivers::Stmpe811::Config& config = {})
        {
            testing::InSequence sequence;
            ExpectReset();
            ExpectIdentification();
            ExpectConfiguration();

            Create(pin, config);
            ForwardTime(Milliseconds(16));
        }

        void CreateAndInitialize(const drivers::Stmpe811::Config& config = {})
        {
            CreateAndInitialize(interruptPin, config);
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
            ExpectPollWithInterrupt(notTouching, 0);
            Start();
            ExecuteAllActions();
        }

        void Interrupt()
        {
            interruptPin.SetStubState(false);
            interruptPin.SetStubState(true);
        }

        void PressAt(uint16_t x, uint16_t y)
        {
            ExpectPollWithInterrupt(touching, 1);
            ExpectFetch(x, y);
            Interrupt();
            ExecuteAllActions();
        }

        void PollWithTouch(uint16_t x, uint16_t y)
        {
            ExpectPollWithInterrupt(touching, 1);
            ExpectFetch(x, y);
            ForwardTime(Milliseconds(10));
        }

        void StartAndPressAt(uint16_t x, uint16_t y)
        {
            StartWithoutTouch();
            PressAt(x, y);
        }

        void StartPositionReadInFlight(uint16_t x, uint16_t y)
        {
            bus.completeAutomatically = false;
            ExpectPollWithInterrupt(touching, 1);
            EXPECT_CALL(bus, ReadRegisterMock(touchDataRegister, 4)).WillOnce(testing::Return(Sample(x, y)));
            Interrupt();
            bus.CompletePending();
            bus.CompletePending();
            bus.CompletePending();
            ExecuteAllActions();
        }

        testing::StrictMock<services::RegisterBusAccessMock> bus;
        hal::GpioPinStub interruptPin;
        std::optional<drivers::Stmpe811> touchScreen;
        std::optional<InitializationResult> initializationResult;
        std::vector<Event> events;
        int stopped{ 0 };
    };

    class Stmpe811PollingTest
        : public Stmpe811Test
    {
    public:
        void ExpectPoll(uint8_t touchControl, uint8_t fifoSize)
        {
            ExpectStatus(touchControl, fifoSize);
        }

        void CreateAndInitializePolling(const drivers::Stmpe811::Config& config = {})
        {
            CreateAndInitialize(hal::dummyPin, config);
        }
    };
}

TEST_F(Stmpe811Test, the_reset_is_released_after_10_milliseconds_and_the_chip_id_is_read_after_another_2)
{
    testing::InSequence sequence;
    ExpectReset();
    Create();
    ExecuteAllActions();
    ForwardTime(Milliseconds(9));

    ExpectWrite(0x03, 0x00);
    ForwardTime(Milliseconds(1));
    ForwardTime(Milliseconds(1));

    EXPECT_CALL(bus, ReadRegisterMock(0x00, 2)).WillOnce(testing::Return(Bytes{ 0x00, 0x00 }));
    ForwardTime(Milliseconds(1));
}

TEST_F(Stmpe811Test, a_device_with_the_chip_id_0x0811_is_configured_and_reported_as_initialized)
{
    testing::InSequence sequence;
    ExpectReset();
    ExpectIdentification();
    ExpectConfiguration();

    Create();
    ForwardTime(Milliseconds(15));
    EXPECT_FALSE(initializationResult);

    ForwardTime(Milliseconds(1));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Stmpe811Test, a_device_with_another_chip_id_is_reported_as_not_found_and_is_not_configured)
{
    testing::InSequence sequence;
    ExpectReset();
    ExpectIdentification({ 0x08, 0x12 });

    Create();
    ForwardTime(Milliseconds(100));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Stmpe811Test, a_chip_id_with_swapped_bytes_is_not_accepted)
{
    testing::InSequence sequence;
    ExpectReset();
    ExpectIdentification({ 0x11, 0x08 });

    Create();
    ForwardTime(Milliseconds(100));

    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::deviceNotFound, *initializationResult);
}

TEST_F(Stmpe811Test, a_device_that_was_not_found_cannot_be_started)
{
    testing::InSequence sequence;
    ExpectReset();
    ExpectIdentification({ 0x00, 0x00 });
    Create();
    ForwardTime(Milliseconds(100));

    EXPECT_DEATH(Start(), "");
}

TEST_F(Stmpe811Test, the_device_cannot_be_started_before_it_is_initialized)
{
    testing::InSequence sequence;
    ExpectReset();
    ExpectIdentification({ 0x00, 0x00 });
    Create();

    EXPECT_DEATH(Start(), "");

    ForwardTime(Milliseconds(100));
}

TEST_F(Stmpe811Test, a_poll_interval_of_zero_is_not_accepted)
{
    EXPECT_DEATH(Create(drivers::Stmpe811::Config{ infra::Duration::zero() }), "");
}

TEST_F(Stmpe811Test, the_size_is_the_12_bit_range_of_the_converter)
{
    CreateAndInitialize();

    EXPECT_EQ((hal::TouchScreenSize{ 4096, 4096 }), touchScreen->Size());
}

TEST_F(Stmpe811Test, the_interrupt_pin_is_made_an_input)
{
    interruptPin.Set(true);
    ASSERT_FALSE(interruptPin.IsInput());

    CreateAndInitialize();

    EXPECT_TRUE(interruptPin.IsInput());
}

TEST_F(Stmpe811Test, Start_samples_at_once_to_find_a_touch_that_is_already_in_progress)
{
    CreateAndInitialize();

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(0x123, 0x456);
    Start();
    ExecuteAllActions();

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 0x123, 0x456 } }), events[0]);
}

TEST_F(Stmpe811Test, without_a_touch_nothing_is_reported_and_the_bus_stays_quiet)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(events.empty());
}

TEST_F(Stmpe811Test, an_interrupt_starts_a_touch_that_is_reported_as_pressed)
{
    CreateAndInitialize();
    StartWithoutTouch();

    PressAt(0x123, 0x456);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 0x123, 0x456 } }), events[0]);
}

TEST_F(Stmpe811Test, the_position_is_12_bits_for_x_and_12_bits_for_y)
{
    CreateAndInitialize();
    StartWithoutTouch();

    PressAt(0xfff, 0x000);
    PollWithTouch(0x000, 0xfff);

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 4095, 0 } }), events[0]);
    EXPECT_EQ((Event{ Phase::moved, { 0, 4095 } }), events[1]);
}

TEST_F(Stmpe811Test, the_position_is_read_from_the_register_that_does_not_increment_its_address)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(touching, 1);
    EXPECT_CALL(bus, ReadRegisterMock(0xd7, 4)).WillOnce(testing::Return(Sample(1, 2)));
    ExpectFifoFlush();
    Interrupt();
    ExecuteAllActions();

    EXPECT_EQ(std::size_t(1), events.size());
}

TEST_F(Stmpe811Test, the_pressure_is_not_part_of_the_position)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(touching, 1);
    EXPECT_CALL(bus, ReadRegisterMock(touchDataRegister, 4)).WillOnce(testing::Return(Sample(0x321, 0x654, 0xff)));
    ExpectFifoFlush();
    Interrupt();
    ExecuteAllActions();

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((hal::TouchPoint{ 0x321, 0x654 }), events[0].point);
}

TEST_F(Stmpe811Test, the_fifo_is_flushed_after_a_sample_is_read_so_the_next_one_is_fresh)
{
    CreateAndInitialize();
    StartWithoutTouch();

    testing::InSequence sequence;
    ExpectPollWithInterrupt(touching, 1);
    EXPECT_CALL(bus, ReadRegisterMock(touchDataRegister, 4)).WillOnce(testing::Return(Sample(1, 2)));
    ExpectWrite(fifoStatusRegister, 0x01);
    ExpectWrite(fifoStatusRegister, 0x00);
    Interrupt();
    ExecuteAllActions();
}

TEST_F(Stmpe811Test, a_touch_in_progress_is_polled_every_10_milliseconds)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    ForwardTime(Milliseconds(9));
    EXPECT_EQ(std::size_t(1), events.size());

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(101, 200);
    ForwardTime(Milliseconds(1));
    PollWithTouch(102, 201);

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::moved, { 101, 200 } }), events[1]);
    EXPECT_EQ((Event{ Phase::moved, { 102, 201 } }), events[2]);
}

TEST_F(Stmpe811Test, the_poll_interval_is_configurable)
{
    CreateAndInitialize(drivers::Stmpe811::Config{ Milliseconds(25) });
    StartAndPressAt(100, 200);

    ForwardTime(Milliseconds(24));
    EXPECT_EQ(std::size_t(1), events.size());

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(150, 200);
    ForwardTime(Milliseconds(1));

    EXPECT_EQ(std::size_t(2), events.size());
}

TEST_F(Stmpe811Test, a_position_that_did_not_change_is_not_reported_again)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    PollWithTouch(100, 200);
    PollWithTouch(100, 200);

    EXPECT_EQ(std::size_t(1), events.size());
}

TEST_F(Stmpe811Test, a_change_in_only_one_axis_is_a_move)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    PollWithTouch(100, 201);
    PollWithTouch(101, 201);

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::moved, { 100, 201 } }), events[1]);
    EXPECT_EQ((Event{ Phase::moved, { 101, 201 } }), events[2]);
}

TEST_F(Stmpe811Test, a_lifted_pen_is_reported_as_released_at_the_last_position)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    PollWithTouch(110, 210);

    ExpectPollWithInterrupt(notTouching, 0);
    ForwardTime(Milliseconds(10));

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::released, { 110, 210 } }), events[2]);
}

TEST_F(Stmpe811Test, polling_stops_after_the_release_until_the_next_interrupt)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    ExpectPollWithInterrupt(notTouching, 0);
    ForwardTime(Milliseconds(10));

    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(std::size_t(2), events.size());
}

TEST_F(Stmpe811Test, a_second_touch_is_pressed_again_at_its_own_position)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    ExpectPollWithInterrupt(notTouching, 0);
    ForwardTime(Milliseconds(10));

    PressAt(100, 200);

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 200 } }), events[2]);
}

TEST_F(Stmpe811Test, a_sample_left_in_the_fifo_by_a_lifted_pen_is_flushed_with_the_release)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    ExpectPollWithInterrupt(notTouching, 1);
    ExpectFifoFlush();
    ForwardTime(Milliseconds(10));

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ(Phase::released, events[1].phase);
}

TEST_F(Stmpe811Test, samples_that_nobody_touched_are_flushed_and_nothing_is_reported)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(notTouching, 2);
    ExpectFifoFlush();
    Interrupt();
    ExecuteAllActions();

    EXPECT_TRUE(events.empty());
}

TEST_F(Stmpe811Test, a_touch_without_a_sample_yet_is_polled_until_the_first_sample_is_ready)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(touching, 0);
    Interrupt();
    ExecuteAllActions();
    EXPECT_TRUE(events.empty());

    PollWithTouch(30, 40);

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 30, 40 } }), events[0]);
}

TEST_F(Stmpe811Test, a_touch_that_ends_before_its_first_sample_is_ready_reports_nothing)
{
    CreateAndInitialize();
    StartWithoutTouch();
    ExpectPollWithInterrupt(touching, 0);
    Interrupt();
    ExecuteAllActions();

    ExpectPollWithInterrupt(notTouching, 0);
    ForwardTime(Milliseconds(10));
    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(events.empty());
}

TEST_F(Stmpe811Test, an_interrupt_during_a_sample_causes_another_sample_afterwards)
{
    CreateAndInitialize();
    StartWithoutTouch();

    testing::InSequence sequence;
    ExpectInterruptStatusCleared();
    EXPECT_CALL(bus, ReadRegisterMock(touchControlRegister, 1)).WillOnce(testing::Invoke([this](uint8_t, std::size_t)
        {
            Interrupt();
            return Bytes{ touching };
        }));
    EXPECT_CALL(bus, ReadRegisterMock(fifoSizeRegister, 1)).WillOnce(testing::Return(Bytes{ 0 }));
    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(7, 8);
    Interrupt();
    ExecuteAllActions();

    ASSERT_EQ(std::size_t(1), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 7, 8 } }), events[0]);
}

TEST_F(Stmpe811Test, an_interrupt_while_the_next_poll_is_waiting_does_not_add_a_sample)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Interrupt();
    ExecuteAllActions();
    ForwardTime(Milliseconds(5));

    PollWithTouch(101, 200);

    EXPECT_EQ(std::size_t(2), events.size());
}

TEST_F(Stmpe811Test, interrupts_are_only_taken_on_the_falling_edge)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(notTouching, 0);
    interruptPin.SetStubState(false);
    ExecuteAllActions();
    testing::Mock::VerifyAndClearExpectations(&bus);

    interruptPin.SetStubState(true);
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Stmpe811Test, Stop_discards_a_touch_in_progress_without_a_released)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);

    Stop();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Stop_is_reported_from_the_event_dispatcher_and_not_from_within_Stop)
{
    CreateAndInitialize();
    StartWithoutTouch();

    Stop();
    EXPECT_EQ(0, stopped);

    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Stop_ignores_the_interrupt_pin)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    Interrupt();
    ExecuteAllActions();

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Start_after_the_stop_is_reported_reports_a_touch_that_is_still_in_progress_as_pressed_again)
{
    CreateAndInitialize();
    StartAndPressAt(100, 200);
    Stop();
    ExecuteAllActions();

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(100, 200);
    Start();
    ExecuteAllActions();

    ASSERT_EQ(std::size_t(2), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 100, 200 } }), events[1]);
}

TEST_F(Stmpe811Test, Start_after_the_stop_is_reported_uses_the_new_callback_only)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();
    ExecuteAllActions();

    int restartedEvents{ 0 };
    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(5, 6);
    touchScreen->Start([&](Event)
        {
            ++restartedEvents;
        });
    ExecuteAllActions();

    EXPECT_EQ(1, restartedEvents);
    EXPECT_TRUE(events.empty());
}

TEST_F(Stmpe811Test, Stop_before_Start_is_reported_and_leaves_the_device_alone)
{
    CreateAndInitialize();

    Stop();
    ExecuteAllActions();
    Interrupt();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Start_twice_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();

    EXPECT_DEATH(Start(), "");
}

TEST_F(Stmpe811Test, Start_before_the_stop_is_reported_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(Start(), "");

    ExecuteAllActions();
}

TEST_F(Stmpe811Test, a_second_Stop_before_the_first_is_reported_is_not_allowed)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(Stop(), "");

    ExecuteAllActions();
}

TEST_F(Stmpe811Test, the_driver_cannot_be_destroyed_before_the_stop_is_reported)
{
    CreateAndInitialize();
    StartWithoutTouch();
    Stop();

    EXPECT_DEATH(touchScreen.reset(), "");

    ExecuteAllActions();
    touchScreen.reset();
}

TEST_F(Stmpe811Test, Stop_during_the_status_read_discards_what_was_read)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectInterruptStatusCleared();
    EXPECT_CALL(bus, ReadRegisterMock(touchControlRegister, 1)).WillOnce(testing::Return(Bytes{ touching }));
    EXPECT_CALL(bus, ReadRegisterMock(fifoSizeRegister, 1)).WillOnce(testing::Invoke([this](uint8_t, std::size_t)
        {
            Stop();
            return Bytes{ 1 };
        }));
    Interrupt();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Stop_during_the_position_read_discards_the_position_and_still_flushes_the_fifo)
{
    CreateAndInitialize();
    StartWithoutTouch();

    ExpectPollWithInterrupt(touching, 1);
    EXPECT_CALL(bus, ReadRegisterMock(touchDataRegister, 4)).WillOnce(testing::Invoke([this](uint8_t, std::size_t)
        {
            Stop();
            return Sample(1, 2);
        }));
    ExpectFifoFlush();
    Interrupt();
    ExecuteAllActions();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_TRUE(events.empty());
    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811Test, Stop_while_the_position_read_is_in_flight_is_reported_when_the_bus_transaction_has_completed)
{
    CreateAndInitialize();
    StartWithoutTouch();

    testing::InSequence sequence;
    StartPositionReadInFlight(30, 40);
    Stop();
    ExecuteAllActions();
    EXPECT_EQ(0, stopped);

    ExpectFifoFlush();
    bus.completeAutomatically = true;
    bus.CompletePending();
    ExecuteAllActions();

    EXPECT_EQ(1, stopped);
    EXPECT_TRUE(events.empty());
    touchScreen.reset();
}

TEST_F(Stmpe811Test, Stop_during_the_initialization_is_reported_when_the_initialization_has_finished)
{
    {
        testing::InSequence sequence;
        ExpectReset();
        ExpectIdentification();
        ExpectConfiguration();
        Create();
        Stop();

        ForwardTime(Milliseconds(15));
        EXPECT_EQ(0, stopped);

        ForwardTime(Milliseconds(1));
    }

    EXPECT_EQ(1, stopped);
    ASSERT_TRUE(initializationResult);
    EXPECT_EQ(InitializationResult::success, *initializationResult);
}

TEST_F(Stmpe811Test, the_consumer_may_stop_from_within_an_event)
{
    CreateAndInitialize();

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(1, 2);
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

TEST_F(Stmpe811Test, the_consumer_may_start_again_from_within_the_stopped_callback)
{
    CreateAndInitialize();

    testing::InSequence sequence;
    int restartedEvents{ 0 };
    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(1, 2);
    touchScreen->Start([&](Event event)
        {
            events.push_back(event);
            touchScreen->Stop([&]()
                {
                    touchScreen->Start([&](Event)
                        {
                            ++restartedEvents;
                        });
                });
        });

    ExpectPollWithInterrupt(touching, 1);
    ExpectFetch(3, 4);
    ExecuteAllActions();

    EXPECT_EQ(std::size_t(1), events.size());
    EXPECT_EQ(1, restartedEvents);
}

TEST_F(Stmpe811Test, destroying_the_driver_disables_the_interrupt)
{
    CreateAndInitialize();
    StartWithoutTouch();

    touchScreen.reset();
    Interrupt();
    ExecuteAllActions();
}

TEST_F(Stmpe811Test, the_driver_cannot_be_destroyed_while_a_bus_transaction_is_outstanding)
{
    CreateAndInitialize();
    StartWithoutTouch();

    bus.completeAutomatically = false;
    ExpectInterruptStatusCleared();
    Interrupt();

    EXPECT_DEATH(touchScreen.reset(), "");

    bus.completeAutomatically = true;
    EXPECT_CALL(bus, ReadRegisterMock(touchControlRegister, 1)).WillOnce(testing::Return(Bytes{ notTouching }));
    EXPECT_CALL(bus, ReadRegisterMock(fifoSizeRegister, 1)).WillOnce(testing::Return(Bytes{ 0 }));
    bus.CompletePending();
    ExecuteAllActions();
}

TEST_F(Stmpe811PollingTest, without_an_interrupt_pin_the_device_is_polled_all_the_time_and_its_interrupt_status_is_left_alone)
{
    CreateAndInitializePolling();

    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();

    ExpectPoll(notTouching, 0);
    ForwardTime(Milliseconds(10));
    ExpectPoll(notTouching, 0);
    ForwardTime(Milliseconds(10));

    EXPECT_TRUE(events.empty());
}

TEST_F(Stmpe811PollingTest, a_touch_is_pressed_moved_and_released_by_polling)
{
    CreateAndInitializePolling();
    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();

    ExpectPoll(touching, 1);
    ExpectFetch(10, 20);
    ForwardTime(Milliseconds(10));
    ExpectPoll(touching, 1);
    ExpectFetch(11, 21);
    ForwardTime(Milliseconds(10));
    ExpectPoll(notTouching, 0);
    ForwardTime(Milliseconds(10));

    ASSERT_EQ(std::size_t(3), events.size());
    EXPECT_EQ((Event{ Phase::pressed, { 10, 20 } }), events[0]);
    EXPECT_EQ((Event{ Phase::moved, { 11, 21 } }), events[1]);
    EXPECT_EQ((Event{ Phase::released, { 11, 21 } }), events[2]);
}

TEST_F(Stmpe811PollingTest, polling_continues_after_the_release)
{
    CreateAndInitializePolling(drivers::Stmpe811::Config{ Milliseconds(20) });
    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();

    for (int poll = 0; poll != 3; ++poll)
    {
        ExpectPoll(notTouching, 0);
        ForwardTime(Milliseconds(20));
    }
}

TEST_F(Stmpe811PollingTest, Stop_ends_the_polling)
{
    CreateAndInitializePolling();
    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();

    Stop();
    ForwardTime(std::chrono::seconds(1));

    EXPECT_EQ(1, stopped);
}

TEST_F(Stmpe811PollingTest, Start_after_the_stop_is_reported_polls_again)
{
    CreateAndInitializePolling();
    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();
    Stop();
    ExecuteAllActions();

    ExpectPoll(notTouching, 0);
    Start();
    ExecuteAllActions();
    ExpectPoll(notTouching, 0);
    ForwardTime(Milliseconds(10));
}
