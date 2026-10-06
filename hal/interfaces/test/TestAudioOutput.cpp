#include "hal/interfaces/test_doubles/AudioOutputStub.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat mono16k{ 16000, 1 };
    constexpr std::size_t samplesPerPeriod = 8;

    class AudioOutputTest
        : public testing::Test
    {
    public:
        void Start(hal::AudioFormat format = stereo48k)
        {
            EXPECT_CALL(stub, Start(format, testing::_, testing::_));

            output.Start(
                format, [this](hal::AudioOutput::Samples toFill)
                {
                    ++periodsRequested;
                    sizeRequested = toFill.size();
                    offeredSilence = std::all_of(toFill.begin(), toFill.end(), [](int16_t sample)
                        {
                            return sample == 0;
                        });
                    std::fill(toFill.begin(), toFill.end(), fillValue);
                },
                [this]()
                {
                    ++underruns;
                });
        }

        testing::StrictMock<hal::AudioOutputStub::WithStorage<64>> stub;
        hal::AudioOutput& output{ stub };
        int16_t fillValue{ 0 };
        int periodsRequested{ 0 };
        std::size_t sizeRequested{ 0 };
        bool offeredSilence{ false };
        int underruns{ 0 };
    };
}

TEST_F(AudioOutputTest, Start_hands_the_requested_format_to_the_implementation)
{
    Start(mono16k);
}

TEST_F(AudioOutputTest, Stop_halts_the_stream)
{
    EXPECT_CALL(stub, Stop());

    output.Stop();
}

TEST_F(AudioOutputTest, no_samples_are_requested_before_a_period_elapses)
{
    Start();

    EXPECT_EQ(0, periodsRequested);
}

TEST_F(AudioOutputTest, one_request_is_made_per_elapsed_period)
{
    Start();

    stub.PeriodElapsed(samplesPerPeriod);
    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(2, periodsRequested);
}

TEST_F(AudioOutputTest, request_offers_a_range_of_the_period_size)
{
    Start();

    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(samplesPerPeriod, sizeRequested);
}

TEST_F(AudioOutputTest, samples_written_by_the_consumer_reach_the_output)
{
    Start();
    fillValue = -1234;

    stub.PeriodElapsed(samplesPerPeriod);

    std::array<int16_t, samplesPerPeriod> expected{};
    expected.fill(-1234);
    EXPECT_TRUE(std::equal(expected.begin(), expected.end(), stub.LastPeriod().begin(), stub.LastPeriod().end()));
}

TEST_F(AudioOutputTest, each_period_is_offered_as_silence_even_if_the_previous_period_was_filled)
{
    Start();
    fillValue = 5;
    stub.PeriodElapsed(samplesPerPeriod);

    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_TRUE(offeredSilence);
}

TEST_F(AudioOutputTest, last_period_has_the_size_of_the_most_recent_request)
{
    Start();
    stub.PeriodElapsed(samplesPerPeriod);

    stub.PeriodElapsed(4);

    EXPECT_EQ(std::size_t(4), stub.LastPeriod().size());
}

TEST_F(AudioOutputTest, underrun_is_reported_through_the_underrun_callback)
{
    Start();

    stub.Underrun();

    EXPECT_EQ(1, underruns);
    EXPECT_EQ(0, periodsRequested);
}

TEST_F(AudioOutputTest, stream_keeps_running_after_an_underrun)
{
    Start();
    stub.Underrun();

    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(1, periodsRequested);
}

TEST_F(AudioOutputTest, no_samples_are_requested_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    output.Stop();

    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(0, periodsRequested);
}

TEST_F(AudioOutputTest, no_underrun_is_reported_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    output.Stop();

    stub.Underrun();

    EXPECT_EQ(0, underruns);
}

TEST_F(AudioOutputTest, consumer_may_stop_the_stream_from_within_a_request)
{
    EXPECT_CALL(stub, Start(stereo48k, testing::_, testing::_));
    output.Start(
        stereo48k, [this](hal::AudioOutput::Samples)
        {
            ++periodsRequested;
            output.Stop();
        },
        []() {});

    EXPECT_CALL(stub, Stop());
    stub.PeriodElapsed(samplesPerPeriod);
    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(1, periodsRequested);
}

TEST_F(AudioOutputTest, restarting_registers_new_callbacks)
{
    Start();
    EXPECT_CALL(stub, Stop());
    output.Stop();

    int restartedPeriods = 0;
    EXPECT_CALL(stub, Start(mono16k, testing::_, testing::_));
    output.Start(
        mono16k, [&restartedPeriods](hal::AudioOutput::Samples)
        {
            ++restartedPeriods;
        },
        []() {});
    stub.PeriodElapsed(samplesPerPeriod);

    EXPECT_EQ(1, restartedPeriods);
    EXPECT_EQ(0, periodsRequested);
}

TEST(AudioFormatTest, formats_with_equal_rate_and_channels_are_equal)
{
    EXPECT_EQ((hal::AudioFormat{ 44100, 2 }), (hal::AudioFormat{ 44100, 2 }));
}

TEST(AudioFormatTest, formats_differing_in_rate_or_channels_are_not_equal)
{
    EXPECT_NE((hal::AudioFormat{ 44100, 2 }), (hal::AudioFormat{ 48000, 2 }));
    EXPECT_NE((hal::AudioFormat{ 44100, 2 }), (hal::AudioFormat{ 44100, 1 }));
}
