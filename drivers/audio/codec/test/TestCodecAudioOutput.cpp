#include "drivers/audio/codec/CodecAudioOutput.hpp"
#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>

namespace
{
    using testing::_;

    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat stereo44k{ 44100, 2 };
    constexpr hal::AudioFormat mono48k{ 48000, 1 };
    constexpr int16_t dirty = 0x5555;

    class CodecFake
        : public drivers::CodecAudioOutput
    {
    public:
        CodecFake(hal::AudioOutput& stream, uint8_t initialVolume)
            : CodecAudioOutput(stream, initialVolume)
        {}

        MOCK_METHOD(void, BeginBringUp, (hal::AudioFormat format), (override));
        MOCK_METHOD(void, BeginApplyLevel, (uint8_t volumePercent, bool muted), (override));
        MOCK_METHOD(void, BeginPowerDown, (), (override));

        bool Supports(hal::AudioFormat format) const override
        {
            return format.channels == 2;
        }

        void Complete()
        {
            SequenceDone();
        }

        bool SequenceInFlight() const
        {
            return Busy();
        }
    };

    class CodecAudioOutputTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        CodecAudioOutputTest()
        {
            EXPECT_CALL(stream, Stop(_)).Times(testing::AtMost(1));
        }

        void Create(uint8_t initialVolume = 100)
        {
            codec.emplace(stream, initialVolume);
        }

        void ExpectStreamStartAndBringUp(hal::AudioFormat format)
        {
            testing::InSequence sequence;
            EXPECT_CALL(stream, Start(format, _, _)).WillOnce(testing::DoAll(testing::SaveArg<1>(&transportSamples), testing::SaveArg<2>(&transportUnderrun)));
            EXPECT_CALL(*codec, BeginBringUp(format));
        }

        void StartWithoutExpectations(hal::AudioFormat format)
        {
            codec->Start(
                format, [this](hal::AudioOutput::Samples toFill)
                {
                    ++periods;
                    offered = toFill;
                    std::fill(toFill.begin(), toFill.end(), fillValue);
                },
                [this]()
                {
                    ++underruns;
                });
        }

        void Start(hal::AudioFormat format = stereo48k)
        {
            ExpectStreamStartAndBringUp(format);
            StartWithoutExpectations(format);
        }

        void ExpectLevel(uint8_t volumePercent, bool muted)
        {
            EXPECT_CALL(*codec, BeginApplyLevel(volumePercent, muted));
        }

        void StartAndWaitUntilPlaying(uint8_t initialVolume = 100)
        {
            Create(initialVolume);
            Start();
            ExpectLevel(initialVolume, false);
            codec->Complete();
            codec->Complete();
        }

        void ExpectStreamStop()
        {
            EXPECT_CALL(stream, Stop(_)).WillOnce(testing::InvokeArgument<0>()).RetiresOnSaturation();
        }

        void ExpectPowerDownAndStreamStop()
        {
            testing::InSequence sequence;
            EXPECT_CALL(*codec, BeginPowerDown());
            ExpectStreamStop();
        }

        void Stop()
        {
            codec->Stop([this]()
                {
                    ++stopped;
                });
        }

        void SetVolume(uint8_t percent)
        {
            codec->SetVolume(percent, [this]()
                {
                    ++volumeApplied;
                });
        }

        void SetMuted(bool muted)
        {
            codec->SetMuted(muted, [this]()
                {
                    ++muteApplied;
                });
        }

        void OfferPeriod(std::size_t numberOfSamples = 8)
        {
            std::fill(buffer.begin(), buffer.end(), dirty);
            transportSamples(infra::Head(infra::MakeRange(buffer), numberOfSamples));
        }

        bool BufferIsSilent() const
        {
            return std::all_of(buffer.begin(), buffer.end(), [](int16_t sample)
                {
                    return sample == 0;
                });
        }

        testing::StrictMock<hal::AudioOutputMock> stream;
        std::optional<testing::StrictMock<CodecFake>> codec;
        infra::Function<void(hal::AudioOutput::Samples)> transportSamples;
        infra::Function<void()> transportUnderrun;
        std::array<int16_t, 8> buffer{};
        hal::AudioOutput::Samples offered;
        int16_t fillValue{ 7 };
        int periods{ 0 };
        int underruns{ 0 };
        int stopped{ 0 };
        int volumeApplied{ 0 };
        int muteApplied{ 0 };
    };
}

TEST_F(CodecAudioOutputTest, constructing_touches_neither_the_stream_nor_the_hooks)
{
    Create();

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, a_volume_above_100_percent_at_construction_is_a_programming_error)
{
    EXPECT_DEATH(Create(101), "");
}

TEST_F(CodecAudioOutputTest, an_unsupported_format_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(StartWithoutExpectations(mono48k), "");
}

TEST_F(CodecAudioOutputTest, starting_while_started_is_a_programming_error)
{
    StartAndWaitUntilPlaying();

    EXPECT_DEATH(StartWithoutExpectations(stereo48k), "");
}

TEST_F(CodecAudioOutputTest, starting_while_shutting_down_is_a_programming_error)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());
    Stop();

    EXPECT_DEATH(StartWithoutExpectations(stereo48k), "");

    ExpectStreamStop();
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, the_stream_is_started_with_the_format_before_the_codec_is_brought_up)
{
    Create();

    Start(stereo44k);
}

TEST_F(CodecAudioOutputTest, the_codec_is_busy_from_the_start_until_the_sequence_is_done)
{
    Create();
    Start();
    EXPECT_TRUE(codec->SequenceInFlight());

    ExpectLevel(100, false);
    codec->Complete();
    EXPECT_TRUE(codec->SequenceInFlight());

    codec->Complete();
    EXPECT_FALSE(codec->SequenceInFlight());
}

TEST_F(CodecAudioOutputTest, the_initial_volume_is_applied_when_bring_up_has_finished)
{
    Create(50);
    Start();

    ExpectLevel(50, false);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, nothing_is_forwarded_to_the_application_during_start_up_and_the_stream_gets_silence)
{
    Create();
    Start();

    OfferPeriod();
    EXPECT_TRUE(BufferIsSilent());
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
}

TEST_F(CodecAudioOutputTest, nothing_is_forwarded_while_the_level_is_being_applied)
{
    Create();
    Start();
    ExpectLevel(100, false);
    codec->Complete();

    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
}

TEST_F(CodecAudioOutputTest, periods_and_underruns_are_forwarded_once_playing)
{
    StartAndWaitUntilPlaying();

    OfferPeriod(6);
    transportUnderrun();

    EXPECT_EQ(1, periods);
    EXPECT_EQ(1, underruns);
    EXPECT_EQ(buffer.data(), offered.begin());
    EXPECT_EQ(std::size_t(6), offered.size());
    EXPECT_EQ(fillValue, buffer.front());
}

TEST_F(CodecAudioOutputTest, stopping_powers_the_codec_down_before_the_stream_is_stopped_and_reports_afterwards)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();

    Stop();
    codec->Complete();

    EXPECT_EQ(0, stopped);
    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, the_stream_keeps_running_until_the_power_down_sequence_is_done)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());

    Stop();
    ExecuteAllActions();

    EXPECT_EQ(0, stopped);
    ExpectStreamStop();
    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, the_completion_is_reported_only_once_the_stream_reports_that_it_has_stopped)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());
    infra::Function<void()> streamStopped;
    EXPECT_CALL(stream, Stop(_)).WillOnce(testing::SaveArg<0>(&streamStopped)).RetiresOnSaturation();
    Stop();
    codec->Complete();
    ExecuteAllActions();

    EXPECT_EQ(0, stopped);
    streamStopped();
    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, stopping_drops_the_application_callbacks_immediately)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());

    Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectStreamStop();
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, stopping_while_the_level_is_being_applied_powers_down_afterwards_and_still_reports_the_level)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(10, false);
    SetVolume(10);

    Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
    ExecuteAllActions();

    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, stopping_without_starting_reports_from_the_event_dispatcher)
{
    Create();

    Stop();
    EXPECT_EQ(0, stopped);

    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, the_application_may_stop_from_within_the_samples_callback)
{
    Create();
    ExpectStreamStartAndBringUp(stereo48k);
    codec->Start(
        stereo48k, [this](hal::AudioOutput::Samples)
        {
            ++periods;
            Stop();
        },
        []() {});
    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();
    EXPECT_CALL(*codec, BeginPowerDown());

    OfferPeriod();
    OfferPeriod();

    EXPECT_EQ(1, periods);
    ExpectStreamStop();
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, the_application_may_stop_from_within_the_underrun_callback)
{
    Create();
    ExpectStreamStartAndBringUp(stereo48k);
    codec->Start(
        stereo48k, [](hal::AudioOutput::Samples) {}, [this]()
        {
            ++underruns;
            Stop();
        });
    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();
    EXPECT_CALL(*codec, BeginPowerDown());

    transportUnderrun();
    transportUnderrun();

    EXPECT_EQ(1, underruns);
    ExpectStreamStop();
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, stopping_during_start_up_powers_down_once_bring_up_has_finished)
{
    Create();
    Start();
    Stop();

    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
    ExecuteAllActions();

    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, starting_again_after_the_completion_uses_the_new_format_and_callbacks)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();
    Stop();
    codec->Complete();
    ExecuteAllActions();

    Start(stereo44k);
    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(CodecAudioOutputTest, starting_before_the_completion_was_reported_is_a_programming_error)
{
    Create();
    Stop();

    EXPECT_DEATH(StartWithoutExpectations(stereo48k), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, stopping_while_a_stop_is_outstanding_is_a_programming_error)
{
    Create();
    Stop();

    EXPECT_DEATH(Stop(), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, destroying_before_the_completion_was_reported_is_a_programming_error)
{
    Create();
    Stop();

    EXPECT_DEATH(codec.reset(), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, stopping_without_a_completion_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->Stop(nullptr), "");
}

TEST_F(CodecAudioOutputTest, setting_the_volume_or_muting_without_a_completion_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->SetVolume(10, nullptr), "");
    EXPECT_DEATH(codec->SetMuted(true, nullptr), "");
}

TEST_F(CodecAudioOutputTest, the_codec_may_be_destroyed_from_the_completion)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();

    codec->Stop([this]()
        {
            codec.reset();
        });
    codec->Complete();
    ExecuteAllActions();

    EXPECT_FALSE(codec.has_value());
}

TEST_F(CodecAudioOutputTest, volume_and_mute_set_before_starting_are_reported_from_the_event_dispatcher_and_applied_at_start_up)
{
    Create();
    SetVolume(25);
    SetMuted(true);
    EXPECT_EQ(0, volumeApplied);
    EXPECT_EQ(0, muteApplied);

    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, muteApplied);

    Start();
    ExpectLevel(25, true);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, the_volume_is_applied_while_playing_and_reported_once_the_sequence_is_done)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(25, false);

    SetVolume(25);
    ExecuteAllActions();
    EXPECT_EQ(0, volumeApplied);

    codec->Complete();
    EXPECT_EQ(0, volumeApplied);
    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(0, muteApplied);
}

TEST_F(CodecAudioOutputTest, muting_is_applied_and_unmuting_restores_the_volume)
{
    StartAndWaitUntilPlaying(40);
    ExpectLevel(40, true);
    SetMuted(true);
    codec->Complete();
    ExecuteAllActions();

    ExpectLevel(40, false);
    SetMuted(false);
    codec->Complete();
    ExecuteAllActions();

    EXPECT_EQ(2, muteApplied);
}

TEST_F(CodecAudioOutputTest, a_volume_set_during_bring_up_is_applied_with_the_initial_level_in_one_go)
{
    Create(100);
    Start();
    SetVolume(35);
    SetMuted(true);

    ExpectLevel(35, true);
    codec->Complete();
    EXPECT_EQ(0, volumeApplied);
    EXPECT_EQ(0, muteApplied);

    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, muteApplied);
}

TEST_F(CodecAudioOutputTest, a_change_made_while_the_level_is_being_applied_is_applied_afterwards)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectLevel(10, false);
    ExpectLevel(10, true);

    SetVolume(10);
    SetMuted(true);

    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(0, muteApplied);

    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, muteApplied);
}

TEST_F(CodecAudioOutputTest, the_next_volume_may_be_set_from_the_completion_of_the_previous_one)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectLevel(10, false);
    ExpectLevel(20, false);

    codec->SetVolume(10, [this]()
        {
            ++volumeApplied;
            SetVolume(20);
        });
    codec->Complete();
    ExecuteAllActions();
    codec->Complete();
    ExecuteAllActions();

    EXPECT_EQ(2, volumeApplied);
}

TEST_F(CodecAudioOutputTest, setting_the_volume_while_one_is_outstanding_is_a_programming_error)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginApplyLevel(_, _)).Times(testing::AtMost(1));
    SetVolume(10);

    EXPECT_DEATH(SetVolume(20), "");

    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, muting_while_a_mute_is_outstanding_is_a_programming_error)
{
    Create();
    SetMuted(true);

    EXPECT_DEATH(SetMuted(false), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, a_volume_above_100_percent_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(SetVolume(101), "");
}

TEST_F(CodecAudioOutputTest, changes_made_during_shutdown_are_reported_without_touching_the_codec_and_applied_at_the_next_start)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();
    Stop();
    SetVolume(40);
    SetMuted(true);
    ExecuteAllActions();

    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, muteApplied);
    codec->Complete();
    ExecuteAllActions();

    Start();
    ExpectLevel(40, true);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, changes_waiting_for_the_bring_up_are_reported_without_being_applied_when_stopping_is_requested)
{
    Create();
    Start();
    SetVolume(40);
    Stop();
    ExecuteAllActions();

    EXPECT_EQ(1, volumeApplied);
    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, changes_made_after_stopping_was_requested_are_reported_without_being_applied)
{
    Create();
    Start();
    Stop();
    SetMuted(true);
    ExecuteAllActions();

    EXPECT_EQ(1, muteApplied);
    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
    ExecuteAllActions();
    EXPECT_EQ(1, stopped);
}

TEST_F(CodecAudioOutputTest, destroying_with_a_level_request_outstanding_is_a_programming_error)
{
    Create();
    SetVolume(40);

    EXPECT_DEATH(codec.reset(), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, destroying_while_playing_stops_the_stream)
{
    StartAndWaitUntilPlaying();

    EXPECT_CALL(stream, Stop(_));
    codec.reset();
}

TEST_F(CodecAudioOutputTest, destroying_while_idle_leaves_the_stream_alone)
{
    Create();

    codec.reset();
}
