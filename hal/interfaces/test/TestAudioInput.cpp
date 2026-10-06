#include "hal/interfaces/test_doubles/AudioInputStub.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat mono16k{ 16000, 1 };

    class AudioInputTest
        : public testing::Test
    {
    public:
        void Start(hal::AudioFormat format = stereo48k)
        {
            EXPECT_CALL(stub, Start(format, testing::_, testing::_));

            input.Start(
                format, [this](hal::AudioInput::Samples samples)
                {
                    ++periodsReceived;
                    received.fill(0);
                    std::copy_n(samples.begin(), std::min(samples.size(), received.size()), received.begin());
                    sizeReceived = samples.size();
                },
                [this]()
                {
                    ++overruns;
                });
        }

        testing::StrictMock<hal::AudioInputStub> stub;
        hal::AudioInput& input{ stub };
        std::array<int16_t, 8> received{};
        int periodsReceived{ 0 };
        std::size_t sizeReceived{ 0 };
        int overruns{ 0 };
    };
}

TEST_F(AudioInputTest, Start_hands_the_requested_format_to_the_implementation)
{
    Start(mono16k);
}

TEST_F(AudioInputTest, Stop_halts_the_stream)
{
    EXPECT_CALL(stub, Stop());

    input.Stop();
}

TEST_F(AudioInputTest, no_samples_are_received_before_a_period_is_captured)
{
    Start();

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(AudioInputTest, captured_samples_reach_the_consumer)
{
    Start();

    std::array<int16_t, 4> captured{ 1, -2, 300, -32768 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(std::size_t(4), sizeReceived);
    EXPECT_EQ(1, received[0]);
    EXPECT_EQ(-2, received[1]);
    EXPECT_EQ(300, received[2]);
    EXPECT_EQ(-32768, received[3]);
}

TEST_F(AudioInputTest, consumer_stays_registered_across_periods)
{
    Start();

    std::array<int16_t, 2> first{ 1, 2 };
    std::array<int16_t, 2> second{ 3, 4 };
    stub.PeriodCaptured(infra::MakeConstRange(first));
    stub.PeriodCaptured(infra::MakeConstRange(second));

    EXPECT_EQ(2, periodsReceived);
    EXPECT_EQ(3, received[0]);
    EXPECT_EQ(4, received[1]);
}

TEST_F(AudioInputTest, overrun_is_reported_through_the_overrun_callback)
{
    Start();

    stub.Overrun();

    EXPECT_EQ(1, overruns);
    EXPECT_EQ(0, periodsReceived);
}

TEST_F(AudioInputTest, stream_keeps_running_after_an_overrun)
{
    Start();
    stub.Overrun();

    std::array<int16_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, periodsReceived);
}

TEST_F(AudioInputTest, no_samples_are_received_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    std::array<int16_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(AudioInputTest, no_overrun_is_reported_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    stub.Overrun();

    EXPECT_EQ(0, overruns);
}

TEST_F(AudioInputTest, consumer_may_stop_the_stream_from_within_a_period)
{
    EXPECT_CALL(stub, Start(stereo48k, testing::_, testing::_));
    input.Start(
        stereo48k, [this](hal::AudioInput::Samples)
        {
            ++periodsReceived;
            input.Stop();
        },
        []() {});

    EXPECT_CALL(stub, Stop());
    std::array<int16_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, periodsReceived);
}

TEST_F(AudioInputTest, restarting_registers_new_callbacks)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    int restartedPeriods = 0;
    EXPECT_CALL(stub, Start(mono16k, testing::_, testing::_));
    input.Start(
        mono16k, [&restartedPeriods](hal::AudioInput::Samples)
        {
            ++restartedPeriods;
        },
        []() {});
    std::array<int16_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, restartedPeriods);
    EXPECT_EQ(0, periodsReceived);
}
