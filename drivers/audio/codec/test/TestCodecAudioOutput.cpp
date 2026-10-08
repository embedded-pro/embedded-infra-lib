#include "drivers/audio/codec/CodecAudioOutput.hpp"
#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
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
            EXPECT_CALL(stream, Stop()).Times(testing::AtMost(1));
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

        void ExpectPowerDownAndStreamStop()
        {
            testing::InSequence sequence;
            EXPECT_CALL(*codec, BeginPowerDown());
            EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
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

TEST_F(CodecAudioOutputTest, stopping_powers_the_codec_down_before_the_stream_is_stopped)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();

    codec->Stop();
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, the_stream_keeps_running_until_the_power_down_sequence_is_done)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());

    codec->Stop();
    ExecuteAllActions();

    testing::Mock::VerifyAndClearExpectations(&stream);
    EXPECT_CALL(stream, Stop());
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, stopping_drops_the_application_callbacks_immediately)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());

    codec->Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
}

TEST_F(CodecAudioOutputTest, stopping_while_the_level_is_being_applied_drops_the_callbacks_and_powers_down_afterwards)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(10, false);
    codec->SetVolume(10);

    codec->Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, stopping_without_starting_does_nothing)
{
    Create();

    codec->Stop();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, the_application_may_stop_from_within_the_samples_callback)
{
    Create();
    ExpectStreamStartAndBringUp(stereo48k);
    codec->Start(
        stereo48k, [this](hal::AudioOutput::Samples)
        {
            ++periods;
            codec->Stop();
        },
        []() {});
    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();
    EXPECT_CALL(*codec, BeginPowerDown());

    OfferPeriod();
    OfferPeriod();

    EXPECT_EQ(1, periods);
}

TEST_F(CodecAudioOutputTest, the_application_may_stop_from_within_the_underrun_callback)
{
    Create();
    ExpectStreamStartAndBringUp(stereo48k);
    codec->Start(
        stereo48k, [](hal::AudioOutput::Samples) {}, [this]()
        {
            ++underruns;
            codec->Stop();
        });
    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();
    EXPECT_CALL(*codec, BeginPowerDown());

    transportUnderrun();
    transportUnderrun();

    EXPECT_EQ(1, underruns);
}

TEST_F(CodecAudioOutputTest, stopping_during_start_up_powers_down_once_bring_up_has_finished)
{
    Create();
    Start();
    codec->Stop();

    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, starting_again_during_shutdown_uses_the_new_format)
{
    StartAndWaitUntilPlaying();
    EXPECT_CALL(*codec, BeginPowerDown());
    codec->Stop();
    StartWithoutExpectations(stereo44k);

    testing::InSequence sequence;
    EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
    EXPECT_CALL(stream, Start(stereo44k, _, _)).WillOnce(testing::DoAll(testing::SaveArg<1>(&transportSamples), testing::SaveArg<2>(&transportUnderrun)));
    EXPECT_CALL(*codec, BeginBringUp(stereo44k));
    codec->Complete();

    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(CodecAudioOutputTest, stopping_and_starting_with_the_same_format_during_start_up_keeps_the_stream_running)
{
    Create();
    Start();
    codec->Stop();
    StartWithoutExpectations(stereo48k);

    ExpectLevel(100, false);
    codec->Complete();
    codec->Complete();

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(CodecAudioOutputTest, stopping_starting_and_stopping_during_start_up_ends_in_shutdown)
{
    Create();
    Start();
    codec->Stop();
    StartWithoutExpectations(stereo48k);
    codec->Stop();

    ExpectPowerDownAndStreamStop();
    codec->Complete();
    codec->Complete();

    OfferPeriod();
    EXPECT_EQ(0, periods);
}

TEST_F(CodecAudioOutputTest, changing_the_format_while_playing_is_not_possible_without_stopping_first)
{
    StartAndWaitUntilPlaying();

    EXPECT_DEATH(StartWithoutExpectations(stereo44k), "");
}

TEST_F(CodecAudioOutputTest, volume_and_mute_set_before_starting_are_applied_at_start_up)
{
    Create();
    codec->SetVolume(25);
    codec->SetMuted(true);
    ExecuteAllActions();
    Start();

    ExpectLevel(25, true);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, changing_the_level_while_idle_does_not_touch_the_codec)
{
    Create();

    codec->SetVolume(25);
    codec->SetMuted(true);
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, volume_is_applied_while_playing)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(25, false);

    codec->SetVolume(25);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, muting_while_playing_applies_the_mute_and_unmuting_restores_the_volume)
{
    StartAndWaitUntilPlaying(40);
    ExpectLevel(40, true);
    codec->SetMuted(true);
    codec->Complete();

    ExpectLevel(40, false);
    codec->SetMuted(false);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, changes_made_while_the_level_is_being_applied_are_coalesced_to_the_latest)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectLevel(10, false);
    ExpectLevel(30, false);

    codec->SetVolume(10);
    codec->SetVolume(20);
    codec->SetVolume(30);
    codec->Complete();
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, a_change_made_while_bring_up_is_running_is_applied_with_the_initial_level_in_one_go)
{
    Create(100);
    Start();
    codec->SetVolume(35);
    codec->SetMuted(true);

    ExpectLevel(35, true);
    codec->Complete();
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, changes_made_during_shutdown_are_applied_at_the_next_start)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownAndStreamStop();
    codec->Stop();
    codec->SetVolume(40);
    codec->SetMuted(true);
    codec->Complete();

    Start();
    ExpectLevel(40, true);
    codec->Complete();
}

TEST_F(CodecAudioOutputTest, a_volume_above_100_percent_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->SetVolume(101), "");
}

TEST_F(CodecAudioOutputTest, destroying_while_playing_stops_the_stream)
{
    StartAndWaitUntilPlaying();

    EXPECT_CALL(stream, Stop());
    codec.reset();
}

TEST_F(CodecAudioOutputTest, destroying_while_idle_leaves_the_stream_alone)
{
    Create();

    codec.reset();
}

TEST_F(CodecAudioOutputTest, stopping_with_a_completion_reports_done_once_the_codec_is_powered_down_and_the_stream_stopped)
{
    StartAndWaitUntilPlaying();
    testing::StrictMock<infra::MockCallback<void()>> stopped;
    testing::InSequence sequence;
    EXPECT_CALL(*codec, BeginPowerDown());
    EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
    EXPECT_CALL(stopped, callback());

    codec->Stop([&stopped]()
        {
            stopped.callback();
        });
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, the_completion_is_not_reported_while_the_codec_is_still_shutting_down)
{
    StartAndWaitUntilPlaying();
    testing::StrictMock<infra::MockCallback<void()>> stopped;
    EXPECT_CALL(*codec, BeginPowerDown());
    codec->Stop([&stopped]()
        {
            stopped.callback();
        });
    ExecuteAllActions();

    testing::InSequence sequence;
    EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
    EXPECT_CALL(stopped, callback());
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, the_completion_is_delivered_from_the_event_dispatcher_even_when_the_codec_is_idle)
{
    Create();
    bool reported = false;

    codec->Stop([&reported]()
        {
            reported = true;
        });
    EXPECT_FALSE(reported);

    ExecuteAllActions();
    EXPECT_TRUE(reported);
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

TEST_F(CodecAudioOutputTest, stopping_with_a_completion_during_start_up_reports_done_after_the_shutdown)
{
    Create();
    Start();
    testing::StrictMock<infra::MockCallback<void()>> stopped;
    codec->Stop([&stopped]()
        {
            stopped.callback();
        });

    testing::InSequence sequence;
    EXPECT_CALL(*codec, BeginPowerDown());
    EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
    EXPECT_CALL(stopped, callback());
    codec->Complete();
    codec->Complete();
    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, starting_before_the_completion_was_reported_is_a_programming_error)
{
    Create();
    codec->Stop([]() {});

    EXPECT_DEATH(StartWithoutExpectations(stereo48k), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, stopping_with_a_completion_while_one_is_pending_is_a_programming_error)
{
    Create();
    codec->Stop([]() {});

    EXPECT_DEATH(codec->Stop([]() {}), "");

    ExecuteAllActions();
}

TEST_F(CodecAudioOutputTest, destroying_before_the_completion_was_reported_is_a_programming_error)
{
    Create();
    codec->Stop([]() {});

    EXPECT_DEATH(codec.reset(), "");

    ExecuteAllActions();
}
