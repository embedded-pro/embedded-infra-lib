#include "drivers/audio/wm8994/Wm8994.hpp"
#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/test_doubles/RegisterBusAccessMock.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

namespace
{
    using testing::_;
    using Output = drivers::Wm8994::Output;

    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat stereo44k{ 44100, 2 };
    constexpr uint16_t rate48k = 0x0083;
    constexpr uint16_t rate44k = 0x0073;
    constexpr uint8_t fullScaleCode = 0xc0;
    constexpr uint16_t volumeUpdate = 0x0100;
    constexpr uint16_t muted = 0x0200;
    constexpr uint16_t unmuted = 0x0010;
    constexpr int16_t dirty = 0x5555;

    std::vector<uint8_t> Word(uint16_t value)
    {
        return { static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value) };
    }

    std::chrono::milliseconds Milliseconds(int count)
    {
        return std::chrono::milliseconds(count);
    }

    class Wm8994Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Wm8994Test()
        {
            EXPECT_CALL(stream, Stop()).Times(testing::AtMost(1));
        }

        void Create(Output output = Output::headphone, uint8_t initialVolume = 100)
        {
            codec.emplace(bus, stream, drivers::Wm8994::Config{ output, initialVolume });
        }

        void ExpectStreamStartAndChipId(hal::AudioFormat format)
        {
            testing::InSequence sequence;
            EXPECT_CALL(stream, Start(format, _, _)).WillOnce(testing::DoAll(testing::SaveArg<1>(&transportSamples), testing::SaveArg<2>(&transportUnderrun)));
            EXPECT_CALL(bus, ReadRegisterMock(0x0000, 2)).WillOnce(testing::Return(Word(0x8994)));
        }

        void Start(hal::AudioFormat format = stereo48k)
        {
            ExpectStreamStartAndChipId(format);
            StartWithoutExpectations(format);
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

        void StartAndWaitUntilPlaying(Output output = Output::headphone, hal::AudioFormat format = stereo48k, uint16_t rate = rate48k, uint8_t code = fullScaleCode)
        {
            Create(output);
            Start(format);
            ExpectStartUp(output, rate, code, false);
            ForwardTime(std::chrono::seconds(1));
        }

        void ExpectWrite(uint16_t address, uint16_t value)
        {
            EXPECT_CALL(bus, WriteRegisterMock(address, Word(value)));
        }

        void ExpectBringUp()
        {
            testing::InSequence sequence;
            ExpectWrite(0x0102, 0x0003);
            ExpectWrite(0x0056, 0x0003);
            ExpectWrite(0x0817, 0x0000);
            ExpectWrite(0x0102, 0x0000);
            ExpectWrite(0x0039, 0x006c);
            ExpectWrite(0x0001, 0x0003);
        }

        void ExpectPath()
        {
            testing::InSequence sequence;
            ExpectWrite(0x0005, 0x0303);
            ExpectWrite(0x0601, 0x0001);
            ExpectWrite(0x0602, 0x0001);
        }

        void ExpectClocking(uint16_t rate)
        {
            testing::InSequence sequence;
            ExpectWrite(0x0210, rate);
            ExpectWrite(0x0300, 0x4010);
            ExpectWrite(0x0208, 0x000a);
            ExpectWrite(0x0200, 0x0001);
        }

        void ExpectOutput(Output output)
        {
            testing::InSequence sequence;
            if (output == Output::headphone)
            {
                ExpectWrite(0x002d, 0x0100);
                ExpectWrite(0x002e, 0x0100);
                ExpectWrite(0x0110, 0x8100);
            }
            else
            {
                ExpectWrite(0x0003, 0x0300);
                ExpectWrite(0x0022, 0x0000);
                ExpectWrite(0x0023, 0x0000);
                ExpectWrite(0x0036, 0x0003);
                ExpectWrite(0x0001, 0x3003);
            }
        }

        void ExpectApply(uint8_t code, bool isMuted)
        {
            testing::InSequence sequence;
            ExpectWrite(0x0610, volumeUpdate | code);
            ExpectWrite(0x0611, volumeUpdate | code);
            ExpectWrite(0x0420, isMuted ? muted : unmuted);
        }

        void ExpectStartUp(Output output, uint16_t rate, uint8_t code, bool isMuted)
        {
            testing::InSequence sequence;
            ExpectBringUp();
            ExpectPath();
            ExpectClocking(rate);
            ExpectOutput(output);
            ExpectApply(code, isMuted);
        }

        void ExpectShutdownMute()
        {
            ExpectWrite(0x0420, muted);
        }

        void ExpectShutdownRest()
        {
            testing::InSequence sequence;
            ExpectWrite(0x002d, 0x0000);
            ExpectWrite(0x002e, 0x0000);
            ExpectWrite(0x0005, 0x0000);
            ExpectWrite(0x0000, 0x0000);
        }

        void ExpectShutdownAndStreamStop()
        {
            testing::InSequence sequence;
            ExpectShutdownMute();
            ExpectShutdownRest();
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

        testing::StrictMock<services::RegisterBusAccessHalfWordMock> bus;
        testing::StrictMock<hal::AudioOutputMock> stream;
        std::optional<drivers::Wm8994> codec;
        infra::Function<void(hal::AudioOutput::Samples)> transportSamples;
        infra::Function<void()> transportUnderrun;
        std::array<int16_t, 8> buffer{};
        hal::AudioOutput::Samples offered;
        int16_t fillValue{ 7 };
        int periods{ 0 };
        int underruns{ 0 };
    };

    struct RateCase
    {
        uint32_t sampleRate;
        uint16_t registerValue;
    };

    const std::array<RateCase, 11> rateCases{ {
        { 8000, 0x0003 },
        { 11025, 0x0013 },
        { 12000, 0x0023 },
        { 16000, 0x0033 },
        { 22050, 0x0043 },
        { 24000, 0x0053 },
        { 32000, 0x0063 },
        { 44100, 0x0073 },
        { 48000, 0x0083 },
        { 88200, 0x0093 },
        { 96000, 0x00a3 },
    } };

    class Wm8994RateTest
        : public Wm8994Test
        , public testing::WithParamInterface<RateCase>
    {};
}

TEST_F(Wm8994Test, constructing_does_not_touch_the_bus_or_the_stream)
{
    Create();

    ExecuteAllActions();
}

TEST_F(Wm8994Test, stereo_at_the_documented_rates_is_supported)
{
    for (uint32_t rate : { 8000u, 11025u, 12000u, 16000u, 22050u, 24000u, 32000u, 44100u, 48000u, 88200u, 96000u })
        EXPECT_TRUE(drivers::Wm8994::IsSupported({ rate, 2 })) << rate;
}

TEST_F(Wm8994Test, mono_and_unknown_rates_are_not_supported)
{
    EXPECT_FALSE(drivers::Wm8994::IsSupported({ 48000, 1 }));
    EXPECT_FALSE(drivers::Wm8994::IsSupported({ 48000, 4 }));
    EXPECT_FALSE(drivers::Wm8994::IsSupported({ 0, 2 }));
    EXPECT_FALSE(drivers::Wm8994::IsSupported({ 44000, 2 }));
    EXPECT_FALSE(drivers::Wm8994::IsSupported({ 192000, 2 }));
}

TEST_F(Wm8994Test, an_unsupported_format_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->Start({ 44000, 2 }, [](hal::AudioOutput::Samples) {}, []() {}), "");
}

TEST_F(Wm8994Test, starting_while_playing_is_a_programming_error)
{
    StartAndWaitUntilPlaying();

    EXPECT_DEATH(StartWithoutExpectations(stereo48k), "");
}

TEST_F(Wm8994Test, the_stream_is_started_with_the_format_before_the_first_bus_access)
{
    Create();
    Start(stereo44k);
    ExpectStartUp(Output::headphone, rate44k, fullScaleCode, false);

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, a_wrong_chip_id_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(
        {
            EXPECT_CALL(stream, Start(_, _, _));
            EXPECT_CALL(bus, ReadRegisterMock(0x0000, 2)).WillOnce(testing::Return(Word(0x1234)));
            StartWithoutExpectations(stereo48k);
            ExecuteAllActions();
        },
        "");
}

TEST_F(Wm8994Test, headphone_start_up_writes_the_registers_in_order)
{
    Create(Output::headphone);
    Start();
    ExpectStartUp(Output::headphone, rate48k, fullScaleCode, false);

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, speaker_start_up_writes_the_registers_in_order)
{
    Create(Output::speaker);
    Start();
    ExpectStartUp(Output::speaker, rate48k, fullScaleCode, false);

    ForwardTime(std::chrono::seconds(1));
}

TEST_P(Wm8994RateTest, the_rate_register_selects_the_sample_rate_at_256_times_fs)
{
    Create();
    Start({ GetParam().sampleRate, 2 });
    ExpectStartUp(Output::headphone, GetParam().registerValue, fullScaleCode, false);

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(SupportedRates, Wm8994RateTest, testing::ValuesIn(rateCases));

TEST_F(Wm8994Test, bring_up_waits_50_ms_for_the_bias_to_settle)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();

    ForwardTime(Milliseconds(49));

    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::headphone);
    ForwardTime(Milliseconds(1));
}

TEST_F(Wm8994Test, headphone_start_up_waits_325_ms_for_the_write_sequencer)
{
    Create(Output::headphone);
    Start();
    ExpectBringUp();
    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::headphone);
    ForwardTime(Milliseconds(50));

    ForwardTime(Milliseconds(324));

    ExpectApply(fullScaleCode, false);
    ForwardTime(Milliseconds(1));
}

TEST_F(Wm8994Test, nothing_is_forwarded_to_the_application_during_start_up_and_the_stream_gets_silence)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();

    OfferPeriod();
    EXPECT_TRUE(BufferIsSilent());
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
}

TEST_F(Wm8994Test, periods_and_underruns_are_forwarded_once_playing)
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

TEST_F(Wm8994Test, stopping_drops_the_application_callbacks_immediately)
{
    StartAndWaitUntilPlaying();
    ExpectWrite(0x0420, muted);

    codec->Stop();
    OfferPeriod();
    transportUnderrun();
    ExecuteAllActions();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
}

TEST_F(Wm8994Test, stopping_while_a_write_is_in_flight_drops_the_application_callbacks_even_though_the_codec_is_still_playing)
{
    StartAndWaitUntilPlaying();
    ExpectApply(drivers::Wm8994::VolumeRegisterValue(10), false);
    codec->SetVolume(10);

    codec->Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectShutdownAndStreamStop();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, stopping_mutes_first_and_stops_the_stream_last)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownAndStreamStop();

    codec->Stop();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, the_stream_keeps_running_while_the_codec_mutes)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownMute();
    codec->Stop();
    ExecuteAllActions();

    ForwardTime(Milliseconds(99));

    testing::InSequence sequence;
    ExpectShutdownRest();
    EXPECT_CALL(stream, Stop()).RetiresOnSaturation();
    ForwardTime(Milliseconds(1));
}

TEST_F(Wm8994Test, stopping_without_starting_does_nothing)
{
    Create();

    codec->Stop();
    ExecuteAllActions();
}

TEST_F(Wm8994Test, the_application_may_stop_from_within_the_samples_callback)
{
    Create();
    ExpectStreamStartAndChipId(stereo48k);
    codec->Start(
        stereo48k, [this](hal::AudioOutput::Samples)
        {
            ++periods;
            codec->Stop();
        },
        []() {});
    ExpectStartUp(Output::headphone, rate48k, fullScaleCode, false);
    ForwardTime(std::chrono::seconds(1));
    ExpectWrite(0x0420, muted);

    OfferPeriod();
    OfferPeriod();
    ExecuteAllActions();

    EXPECT_EQ(1, periods);
}

TEST_F(Wm8994Test, the_application_may_stop_from_within_the_underrun_callback)
{
    Create();
    ExpectStreamStartAndChipId(stereo48k);
    codec->Start(
        stereo48k, [](hal::AudioOutput::Samples) {}, [this]()
        {
            ++underruns;
            codec->Stop();
        });
    ExpectStartUp(Output::headphone, rate48k, fullScaleCode, false);
    ForwardTime(std::chrono::seconds(1));
    ExpectWrite(0x0420, muted);

    transportUnderrun();
    transportUnderrun();
    ExecuteAllActions();

    EXPECT_EQ(1, underruns);
}

TEST_F(Wm8994Test, stopping_during_start_up_shuts_down_once_start_up_has_finished)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();
    codec->Stop();

    testing::InSequence sequence;
    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::headphone);
    ExpectShutdownAndStreamStop();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, stopping_while_a_bus_transaction_is_in_flight_waits_for_it)
{
    Create(Output::speaker);
    bus.completeAutomatically = false;
    Start();
    codec->Stop();
    bus.completeAutomatically = true;

    testing::InSequence sequence;
    ExpectBringUp();
    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::speaker);
    ExpectShutdownAndStreamStop();
    bus.CompletePending();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, starting_again_during_shutdown_uses_the_new_format)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownAndStreamStop();
    codec->Stop();
    StartWithoutExpectations(stereo44k);

    testing::InSequence sequence;
    EXPECT_CALL(stream, Start(stereo44k, _, _)).WillOnce(testing::DoAll(testing::SaveArg<1>(&transportSamples), testing::SaveArg<2>(&transportUnderrun)));
    EXPECT_CALL(bus, ReadRegisterMock(0x0000, 2)).WillOnce(testing::Return(Word(0x8994)));
    ExpectStartUp(Output::headphone, rate44k, fullScaleCode, false);
    ForwardTime(std::chrono::seconds(2));

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(Wm8994Test, stopping_and_starting_with_the_same_format_during_start_up_keeps_the_stream_running)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();
    codec->Stop();
    StartWithoutExpectations(stereo48k);

    testing::InSequence sequence;
    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::headphone);
    ExpectApply(fullScaleCode, false);
    ForwardTime(std::chrono::seconds(1));

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(Wm8994Test, stopping_starting_and_stopping_during_start_up_ends_in_shutdown)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();
    codec->Stop();
    StartWithoutExpectations(stereo48k);
    codec->Stop();

    testing::InSequence sequence;
    ExpectPath();
    ExpectClocking(rate48k);
    ExpectOutput(Output::headphone);
    ExpectShutdownAndStreamStop();
    ForwardTime(std::chrono::seconds(1));

    OfferPeriod();
    EXPECT_EQ(0, periods);
}

TEST_F(Wm8994Test, the_initial_volume_is_applied_at_the_end_of_start_up)
{
    Create(Output::headphone, 50);
    Start();

    ExpectStartUp(Output::headphone, rate48k, drivers::Wm8994::VolumeRegisterValue(50), false);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, volume_and_mute_set_before_starting_are_applied_at_start_up)
{
    Create();
    codec->SetVolume(25);
    codec->SetMuted(true);
    ExecuteAllActions();
    Start();

    ExpectStartUp(Output::headphone, rate48k, drivers::Wm8994::VolumeRegisterValue(25), true);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, volume_is_written_to_both_channels_with_the_update_bit_while_playing)
{
    StartAndWaitUntilPlaying();
    ExpectApply(drivers::Wm8994::VolumeRegisterValue(25), false);

    codec->SetVolume(25);
    ExecuteAllActions();
}

TEST_F(Wm8994Test, muting_while_playing_soft_mutes_and_unmuting_restores_the_volume)
{
    StartAndWaitUntilPlaying();
    ExpectApply(fullScaleCode, true);
    codec->SetMuted(true);
    ExecuteAllActions();

    ExpectApply(fullScaleCode, false);
    codec->SetMuted(false);
    ExecuteAllActions();
}

TEST_F(Wm8994Test, changes_made_while_a_write_is_in_flight_are_coalesced_to_the_latest)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectApply(drivers::Wm8994::VolumeRegisterValue(10), false);
    ExpectApply(drivers::Wm8994::VolumeRegisterValue(30), false);

    codec->SetVolume(10);
    codec->SetVolume(20);
    codec->SetVolume(30);
    ExecuteAllActions();
}

TEST_F(Wm8994Test, a_volume_of_zero_followed_by_unmuting_writes_the_lowest_code_and_the_unmute)
{
    StartAndWaitUntilPlaying();
    ExpectApply(fullScaleCode, true);
    codec->SetMuted(true);
    ExecuteAllActions();
    ExpectApply(0, true);
    codec->SetVolume(0);
    ExecuteAllActions();

    ExpectApply(0, false);
    codec->SetMuted(false);
    ExecuteAllActions();
}

TEST_F(Wm8994Test, changes_made_during_shutdown_are_applied_at_the_next_start)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownAndStreamStop();
    codec->Stop();
    codec->SetVolume(40);
    codec->SetMuted(true);
    ForwardTime(std::chrono::seconds(1));

    Start();
    ExpectStartUp(Output::headphone, rate48k, drivers::Wm8994::VolumeRegisterValue(40), true);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Wm8994Test, a_volume_above_100_percent_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->SetVolume(101), "");
}

TEST(Wm8994VolumeTest, zero_percent_is_the_codec_mute_code)
{
    EXPECT_EQ(0, drivers::Wm8994::VolumeRegisterValue(0));
}

TEST(Wm8994VolumeTest, one_percent_is_not_the_mute_code)
{
    EXPECT_GT(drivers::Wm8994::VolumeRegisterValue(1), 0);
}

TEST(Wm8994VolumeTest, one_hundred_percent_is_0_dB)
{
    EXPECT_EQ(fullScaleCode, drivers::Wm8994::VolumeRegisterValue(100));
}

TEST(Wm8994VolumeTest, the_code_never_decreases_with_the_percentage)
{
    for (uint8_t percent = 1; percent <= 100; ++percent)
        EXPECT_GE(drivers::Wm8994::VolumeRegisterValue(percent), drivers::Wm8994::VolumeRegisterValue(static_cast<uint8_t>(percent - 1))) << int(percent);
}

TEST_F(Wm8994Test, destroying_while_playing_stops_the_stream)
{
    StartAndWaitUntilPlaying();

    EXPECT_CALL(stream, Stop());
    codec.reset();
}

TEST_F(Wm8994Test, destroying_while_idle_leaves_the_stream_alone)
{
    Create();

    codec.reset();
}

TEST_F(Wm8994Test, destroying_while_waiting_for_a_delay_stops_the_stream_and_cancels_the_wait)
{
    Create();
    Start();
    ExpectBringUp();
    ExecuteAllActions();

    EXPECT_CALL(stream, Stop());
    codec.reset();

    ForwardTime(std::chrono::seconds(1));
}
