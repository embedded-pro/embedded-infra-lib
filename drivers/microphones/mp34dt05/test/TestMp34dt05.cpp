#include "drivers/microphones/mp34dt05/Mp34dt05.hpp"
#include "drivers/microphones/pdm/test_doubles/PdmToPcmMock.hpp"
#include "hal/interfaces/test_doubles/AudioInputStub.hpp"
#include "infra/event/test_helper/EventDispatcherFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace
{
    struct Scenario
    {
        hal::AudioFormat format;
        uint16_t decimation;
        hal::AudioFormat sourceFormat;
        std::size_t startupSamples;
    };

    constexpr Scenario mono16k{ { 16000, 1 }, 128, { 128000, 1 }, 320 };
    constexpr Scenario stereo48k{ { 48000, 2 }, 64, { 192000, 2 }, 1920 };

    class Mp34dt05Test
        : public testing::Test
        , public infra::EventDispatcherFixture
    {
    public:
        Mp34dt05Test()
        {
            EXPECT_CALL(converter, Decimation()).WillRepeatedly(testing::ReturnPointee(&decimation));
            EXPECT_CALL(converter, MaxSamples(testing::_)).WillRepeatedly([this](std::size_t)
                {
                    return producing;
                });
            EXPECT_CALL(converter, Convert(testing::_, testing::_)).WillRepeatedly([this](infra::MemoryRange<const int16_t> words, infra::MemoryRange<int16_t> samples)
                {
                    convertedWords = words;
                    convertedInto = samples;
                    std::generate_n(samples.begin(), producing, [this]()
                        {
                            return nextValue++;
                        });
                    return producing;
                });

            driver.emplace(input, converter, infra::MakeRange(storage));
        }

        ~Mp34dt05Test() override
        {
            EXPECT_CALL(input, Stop(testing::_)).Times(testing::AnyNumber());
            driver.reset();
        }

        void ExpectStart(const Scenario& scenario)
        {
            decimation = scenario.decimation;

            testing::InSequence sequence;
            EXPECT_CALL(converter, Reset(scenario.format.channels, scenario.format.sampleRate));
            EXPECT_CALL(input, Start(scenario.sourceFormat, testing::_, testing::_));
        }

        void Start(const Scenario& scenario = mono16k)
        {
            ExpectStart(scenario);

            driver->Start(
                scenario.format, [this](hal::AudioInput::Samples samples)
                {
                    ++periodsReceived;
                    receivedSize = samples.size();
                    std::copy_n(samples.begin(), std::min(samples.size(), received.size()), received.begin());
                },
                [this]()
                {
                    ++overruns;
                });
        }

        void StartAndPassStartup(const Scenario& scenario = mono16k)
        {
            Start(scenario);
            Capture(scenario.startupSamples);
            periodsReceived = 0;
        }

        void Capture(std::size_t samples)
        {
            producing = samples;
            input.PeriodCaptured(infra::MakeConstRange(captured));
        }

        testing::StrictMock<hal::AudioInputStub> input;
        testing::StrictMock<drivers::PdmToPcmMock> converter;
        std::array<int16_t, 4096> storage{};
        std::array<int16_t, 16> captured{};
        std::optional<drivers::Mp34dt05> driver;

        uint16_t decimation{ 0 };
        std::size_t producing{ 0 };
        int16_t nextValue{ 1 };
        infra::MemoryRange<const int16_t> convertedWords;
        infra::MemoryRange<int16_t> convertedInto;

        std::array<int16_t, 4096> received{};
        std::size_t receivedSize{ 0 };
        int periodsReceived{ 0 };
        int overruns{ 0 };
        int restartedPeriods{ 0 };
        int restartedOverruns{ 0 };
    };
}

TEST_F(Mp34dt05Test, Start_derives_the_clock_from_the_sample_rate_and_the_decimation_of_the_converter)
{
    Start(stereo48k);
}

TEST_F(Mp34dt05Test, Start_asks_the_bit_stream_for_one_16_bit_word_per_16_clock_cycles_of_each_microphone)
{
    Start(mono16k);
}

TEST_F(Mp34dt05Test, Start_resets_the_converter_before_starting_the_input)
{
    Start();
}

TEST_F(Mp34dt05Test, Start_accepts_the_lowest_supported_clock)
{
    decimation = 64;
    EXPECT_CALL(converter, Reset(1, 20000));
    EXPECT_CALL(input, Start(hal::AudioFormat{ drivers::Mp34dt05::minClockFrequency / 16, 1 }, testing::_, testing::_));

    driver->Start({ 20000, 1 }, [](hal::AudioInput::Samples) {}, []() {});
}

TEST_F(Mp34dt05Test, Start_accepts_the_highest_supported_clock)
{
    decimation = 64;
    EXPECT_CALL(converter, Reset(1, 50781));
    EXPECT_CALL(input, Start(hal::AudioFormat{ 203124, 1 }, testing::_, testing::_));

    driver->Start({ 50781, 1 }, [](hal::AudioInput::Samples) {}, []() {});
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

TEST_F(Mp34dt05Test, Start_rejects_a_clock_that_overflows_32_bits)
{
    decimation = 65535;

    EXPECT_DEATH(driver->Start({ 4294967295, 1 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, Start_rejects_a_clock_that_is_not_a_multiple_of_the_word_size)
{
    decimation = 63;

    EXPECT_DEATH(driver->Start({ 30001, 1 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, Start_rejects_zero_channels)
{
    decimation = 64;

    EXPECT_DEATH(driver->Start({ 48000, 0 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, Start_rejects_more_than_two_channels)
{
    decimation = 64;

    EXPECT_DEATH(driver->Start({ 48000, 3 }, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, Start_while_running_is_rejected)
{
    Start();

    EXPECT_DEATH(driver->Start(mono16k.format, [](hal::AudioInput::Samples) {}, []() {}), "");
}

TEST_F(Mp34dt05Test, no_samples_are_received_before_a_period_is_captured)
{
    Start();

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, converter_receives_the_captured_words_and_the_buffer_of_the_driver)
{
    StartAndPassStartup();

    Capture(4);

    EXPECT_EQ(captured.data(), convertedWords.begin());
    EXPECT_EQ(captured.size(), convertedWords.size());
    EXPECT_EQ(storage.data(), convertedInto.begin());
    EXPECT_EQ(storage.size(), convertedInto.size());
}

TEST_F(Mp34dt05Test, the_word_count_asked_of_the_converter_is_the_number_of_captured_words)
{
    Start();
    EXPECT_CALL(converter, MaxSamples(captured.size())).WillOnce(testing::Return(0));
    EXPECT_CALL(converter, Convert(testing::_, testing::_)).WillOnce(testing::Return(0));

    input.PeriodCaptured(infra::MakeConstRange(captured));
}

TEST_F(Mp34dt05Test, samples_produced_after_the_startup_reach_the_consumer)
{
    StartAndPassStartup();
    nextValue = 100;

    Capture(4);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(std::size_t(4), receivedSize);
    EXPECT_EQ(100, received[0]);
    EXPECT_EQ(101, received[1]);
    EXPECT_EQ(102, received[2]);
    EXPECT_EQ(103, received[3]);
}

TEST_F(Mp34dt05Test, samples_produced_during_the_startup_are_discarded)
{
    Start();

    Capture(mono16k.startupSamples - 1);

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, only_the_samples_beyond_the_startup_are_delivered)
{
    Start();

    Capture(mono16k.startupSamples + 3);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(std::size_t(3), receivedSize);
    EXPECT_EQ(int16_t(mono16k.startupSamples + 1), received[0]);
    EXPECT_EQ(int16_t(mono16k.startupSamples + 3), received[2]);
}

TEST_F(Mp34dt05Test, the_startup_is_discarded_across_periods)
{
    Start();

    Capture(200);
    Capture(200);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(std::size_t(80), receivedSize);
    EXPECT_EQ(int16_t(321), received[0]);
}

TEST_F(Mp34dt05Test, an_empty_period_is_not_delivered)
{
    StartAndPassStartup();

    Capture(0);

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, the_startup_covers_every_channel)
{
    Start(stereo48k);

    Capture(stereo48k.startupSamples + 2);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(std::size_t(2), receivedSize);
    EXPECT_EQ(int16_t(stereo48k.startupSamples + 1), received[0]);
    EXPECT_EQ(int16_t(stereo48k.startupSamples + 2), received[1]);
}

TEST_F(Mp34dt05Test, the_startup_is_not_repeated_between_periods)
{
    StartAndPassStartup();

    Capture(1);
    Capture(1);

    EXPECT_EQ(2, periodsReceived);
}

TEST_F(Mp34dt05Test, an_output_larger_than_the_buffer_of_the_driver_is_rejected)
{
    StartAndPassStartup();
    producing = storage.size() + 1;

    EXPECT_DEATH(input.PeriodCaptured(infra::MakeConstRange(captured)), "");
}

TEST_F(Mp34dt05Test, overrun_is_reported_through_the_overrun_callback)
{
    Start();

    input.Overrun();

    EXPECT_EQ(1, overruns);
    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, stream_keeps_running_after_an_overrun)
{
    StartAndPassStartup();
    input.Overrun();

    Capture(2);

    EXPECT_EQ(1, periodsReceived);
}

TEST_F(Mp34dt05Test, Stop_stops_the_input)
{
    Start();
    EXPECT_CALL(input, Stop(testing::_));

    driver->Stop(infra::emptyFunction);
}

TEST_F(Mp34dt05Test, Stop_reports_when_the_input_has_stopped)
{
    Start();
    EXPECT_CALL(input, Stop(testing::_));
    bool stopped = false;

    driver->Stop([&stopped]()
        {
            stopped = true;
        });

    EXPECT_TRUE(stopped);
}

TEST_F(Mp34dt05Test, Stop_without_a_running_stream_reports_done_from_the_event_dispatcher)
{
    bool stopped = false;

    driver->Stop([&stopped]()
        {
            stopped = true;
        });
    EXPECT_FALSE(stopped);

    ExecuteAllActions();
    EXPECT_TRUE(stopped);
}

TEST_F(Mp34dt05Test, no_samples_are_received_after_Stop)
{
    StartAndPassStartup();
    EXPECT_CALL(input, Stop(testing::_));
    driver->Stop(infra::emptyFunction);

    Capture(2);

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, no_overrun_is_reported_after_Stop)
{
    Start();
    EXPECT_CALL(input, Stop(testing::_));
    driver->Stop(infra::emptyFunction);

    input.Overrun();

    EXPECT_EQ(0, overruns);
}

TEST_F(Mp34dt05Test, consumer_may_stop_the_stream_from_within_a_period)
{
    int periods{ 0 };
    ExpectStart(mono16k);
    driver->Start(
        mono16k.format, [this, &periods](hal::AudioInput::Samples)
        {
            ++periods;
            driver->Stop(infra::emptyFunction);
        },
        []() {});
    Capture(mono16k.startupSamples);

    EXPECT_CALL(input, Stop(testing::_));
    Capture(2);
    Capture(2);

    EXPECT_EQ(1, periods);
}

TEST_F(Mp34dt05Test, consumer_may_stop_the_stream_from_within_an_overrun)
{
    int reported{ 0 };
    ExpectStart(mono16k);
    driver->Start(
        mono16k.format, [](hal::AudioInput::Samples) {}, [this, &reported]()
        {
            ++reported;
            driver->Stop(infra::emptyFunction);
        });

    EXPECT_CALL(input, Stop(testing::_));
    input.Overrun();
    input.Overrun();

    EXPECT_EQ(1, reported);
}

TEST_F(Mp34dt05Test, consumer_may_restart_the_stream_from_within_a_period)
{
    ExpectStart(mono16k);
    driver->Start(
        mono16k.format, [&counter = periodsReceived, this](hal::AudioInput::Samples)
        {
            EXPECT_CALL(input, Stop(testing::_));
            driver->Stop(infra::emptyFunction);
            ExpectStart(mono16k);
            driver->Start(
                mono16k.format, [&counter = restartedPeriods](hal::AudioInput::Samples)
                {
                    ++counter;
                },
                []() {});
            ++counter;
        },
        []() {});
    Capture(mono16k.startupSamples + 1);

    Capture(mono16k.startupSamples + 1);

    EXPECT_EQ(1, periodsReceived);
    EXPECT_EQ(1, restartedPeriods);
}

TEST_F(Mp34dt05Test, consumer_may_restart_the_stream_from_within_an_overrun)
{
    ExpectStart(mono16k);
    driver->Start(
        mono16k.format, [](hal::AudioInput::Samples) {}, [&counter = overruns, this]()
        {
            EXPECT_CALL(input, Stop(testing::_));
            driver->Stop(infra::emptyFunction);
            ExpectStart(mono16k);
            driver->Start(
                mono16k.format, [](hal::AudioInput::Samples) {}, [&counter = restartedOverruns]()
                {
                    ++counter;
                });
            ++counter;
        });
    input.Overrun();

    input.Overrun();

    EXPECT_EQ(1, overruns);
    EXPECT_EQ(1, restartedOverruns);
}

TEST_F(Mp34dt05Test, restarting_discards_the_startup_again)
{
    StartAndPassStartup();
    EXPECT_CALL(input, Stop(testing::_));
    driver->Stop(infra::emptyFunction);

    Start();
    Capture(mono16k.startupSamples - 1);

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, restarting_registers_the_new_callbacks)
{
    StartAndPassStartup();
    EXPECT_CALL(input, Stop(testing::_));
    driver->Stop(infra::emptyFunction);

    ExpectStart(mono16k);
    driver->Start(
        mono16k.format, [this](hal::AudioInput::Samples)
        {
            ++restartedPeriods;
        },
        []() {});
    Capture(mono16k.startupSamples + 1);

    EXPECT_EQ(1, restartedPeriods);
    EXPECT_EQ(0, periodsReceived);
}

TEST_F(Mp34dt05Test, destroying_a_running_driver_stops_the_input)
{
    Start();
    EXPECT_CALL(input, Stop(testing::_));

    driver.reset();
}

TEST_F(Mp34dt05Test, destroying_an_idle_driver_leaves_the_input_alone)
{
    driver.reset();
}

TEST_F(Mp34dt05Test, destroying_a_stopped_driver_does_not_stop_the_input_again)
{
    Start();
    EXPECT_CALL(input, Stop(testing::_));
    driver->Stop(infra::emptyFunction);

    driver.reset();
}
