#include "drivers/microphones/mp34dt05/Mp34dt05.hpp"
#include "drivers/microphones/pdm/test_doubles/PdmMicrophoneChipTest.hpp"
#include "hal/interfaces/AudioInput.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace
{
    using Mp34dt05Test = drivers::PdmMicrophoneChipTest<drivers::Mp34dt05>;
}

TEST_F(Mp34dt05Test, has_the_clock_range_and_the_startup_time_of_the_chip)
{
    EXPECT_EQ(1280000u, drivers::Mp34dt05::minClockFrequency);
    EXPECT_EQ(3250000u, drivers::Mp34dt05::maxClockFrequency);
    EXPECT_EQ(20u, drivers::Mp34dt05::startupTimeInMilliseconds);
}

TEST_F(Mp34dt05Test, Start_accepts_the_lowest_supported_clock)
{
    Start({ 20000, 1 }, 64, { 80000, 1 });
}

TEST_F(Mp34dt05Test, Start_accepts_the_highest_supported_clock)
{
    Start({ 50781, 1 }, 64, { 203124, 1 });
}

TEST_F(Mp34dt05Test, Start_rejects_a_clock_below_the_supported_range)
{
    decimation = 64;

    EXPECT_DEATH(driver->Start({ 19999, 1 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, Start_rejects_a_clock_above_the_supported_range)
{
    decimation = 64;

    EXPECT_DEATH(driver->Start({ 50782, 1 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, the_first_20_milliseconds_of_a_stream_are_discarded)
{
    Start({ 16000, 1 }, 128, { 128000, 1 });

    Capture(320);

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, the_first_sample_after_20_milliseconds_is_delivered)
{
    Start({ 16000, 1 }, 128, { 128000, 1 });
    Capture(320);

    Capture(1);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(std::size_t(1), receivedSize);
}
