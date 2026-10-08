#include "drivers/audio/cs43l22/Cs43l22.hpp"
#include "drivers/audio/cs43l22/test/Cs43l22BusMock.hpp"
#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "hal/interfaces/test_doubles/GpioMock.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/test_helper/MockCallback.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>

namespace
{
    using testing::_;
    using AnalogInput = drivers::Cs43l22::AnalogInput;
    using Output = drivers::Cs43l22::Output;

    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat stereo44k{ 44100, 2 };
    constexpr uint8_t chipIdRevisionB1 = 0xe3;
    constexpr uint8_t fullScaleCode = 0x00;
    constexpr uint8_t outputsOff = 0xff;
    constexpr int16_t dirty = 0x5555;

    std::chrono::milliseconds Milliseconds(int count)
    {
        return std::chrono::milliseconds(count);
    }

    uint8_t OutputsValue(Output output)
    {
        switch (output)
        {
            case Output::headphone:
                return 0xaf;
            case Output::speaker:
                return 0xfa;
            case Output::both:
                return 0xaa;
            case Output::automatic:
                return 0x05;
        }

        return 0;
    }

    class Cs43l22Test
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Cs43l22Test()
        {
            EXPECT_CALL(stream, Stop(_)).Times(testing::AtMost(1));
            EXPECT_CALL(reset, ResetConfig()).Times(testing::AtMost(1));
        }

        void Create(Output output = Output::headphone, uint8_t initialVolume = 100, std::optional<AnalogInput> passthrough = std::nullopt)
        {
            EXPECT_CALL(reset, Config(hal::PinConfigType::output, false));
            codec.emplace(bus, stream, reset, drivers::Cs43l22::Config{ output, initialVolume, passthrough });
        }

        void Start(hal::AudioFormat format = stereo48k)
        {
            testing::InSequence sequence;
            EXPECT_CALL(stream, Start(format, _, _)).WillOnce(testing::DoAll(testing::SaveArg<1>(&transportSamples), testing::SaveArg<2>(&transportUnderrun)));
            EXPECT_CALL(reset, Set(false));
            StartWithoutExpectations(format);
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

        void SetMuted(bool isMuted)
        {
            codec->SetMuted(isMuted, [this]()
                {
                    ++muteApplied;
                });
        }

        void ExpectStreamStop()
        {
            EXPECT_CALL(stream, Stop(_)).WillOnce(testing::InvokeArgument<0>()).RetiresOnSaturation();
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

        void StartAndWaitUntilPlaying(Output output = Output::headphone, uint8_t volume = 100, std::optional<AnalogInput> passthrough = std::nullopt)
        {
            Create(output, volume, passthrough);
            Start();
            ExpectStartUp(output, drivers::Cs43l22::VolumeRegisterValue(volume), false, passthrough);
            ForwardTime(std::chrono::seconds(1));
        }

        void ExpectWrite(uint8_t address, uint8_t value)
        {
            EXPECT_CALL(bus, WriteRegisterMock(address, value));
        }

        void ExpectModify(uint8_t address, uint8_t before, uint8_t after)
        {
            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(address)).WillOnce(testing::Return(before));
            ExpectWrite(address, after);
        }

        void ExpectIdentification(uint8_t chipId = chipIdRevisionB1)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x01)).WillOnce(testing::Return(chipId));
        }

        void ExpectRequiredInitialization()
        {
            testing::InSequence sequence;
            ExpectWrite(0x02, 0x01);
            ExpectWrite(0x00, 0x99);
            ExpectWrite(0x47, 0x80);
            ExpectModify(0x32, 0x12, 0x92);
            ExpectModify(0x32, 0x92, 0x12);
            ExpectWrite(0x00, 0x00);
        }

        void ExpectConfiguration()
        {
            testing::InSequence sequence;
            ExpectWrite(0x04, outputsOff);
            ExpectWrite(0x05, 0x81);
            ExpectWrite(0x06, 0x04);
            ExpectWrite(0x0f, 0x00);
        }

        void ExpectPassthrough(AnalogInput input)
        {
            const uint8_t select = static_cast<uint8_t>(1 << static_cast<uint8_t>(input));

            testing::InSequence sequence;
            ExpectWrite(0x08, select);
            ExpectWrite(0x09, select);
            ExpectModify(0x0e, 0x04, 0xc4);
        }

        void ExpectPowerUp()
        {
            ExpectWrite(0x02, 0x9e);
        }

        void ExpectBringUp(std::optional<AnalogInput> passthrough = std::nullopt)
        {
            testing::InSequence sequence;
            EXPECT_CALL(reset, Set(true));
            ExpectIdentification();
            ExpectRequiredInitialization();
            ExpectConfiguration();
            if (passthrough)
                ExpectPassthrough(*passthrough);
            ExpectPowerUp();
        }

        void ExpectLevel(uint8_t volumeCode, uint8_t outputs)
        {
            testing::InSequence sequence;
            ExpectWrite(0x20, volumeCode);
            ExpectWrite(0x21, volumeCode);
            ExpectWrite(0x04, outputs);
        }

        void ExpectStartUp(Output output, uint8_t volumeCode, bool isMuted, std::optional<AnalogInput> passthrough = std::nullopt)
        {
            testing::InSequence sequence;
            ExpectBringUp(passthrough);
            ExpectLevel(volumeCode, isMuted ? outputsOff : OutputsValue(output));
        }

        void ExpectPowerDownWrites()
        {
            testing::InSequence sequence;
            ExpectWrite(0x04, outputsOff);
            ExpectWrite(0x02, 0x01);
        }

        void ExpectResetAssertedAndStreamStopped()
        {
            testing::InSequence sequence;
            EXPECT_CALL(reset, Set(false));
            ExpectStreamStop();
        }

        void ExpectShutdownAndStreamStop()
        {
            testing::InSequence sequence;
            ExpectPowerDownWrites();
            ExpectResetAssertedAndStreamStopped();
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

        testing::StrictMock<drivers::Cs43l22BusMock> bus;
        testing::StrictMock<hal::AudioOutputMock> stream;
        testing::StrictMock<hal::GpioPinMock> reset;
        std::optional<drivers::Cs43l22> codec;
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

    const std::array<Output, 4> outputs{ Output::headphone, Output::speaker, Output::both, Output::automatic };

    class Cs43l22OutputTest
        : public Cs43l22Test
        , public testing::WithParamInterface<Output>
    {};

    const std::array<AnalogInput, 4> analogInputs{ AnalogInput::ain1, AnalogInput::ain2, AnalogInput::ain3, AnalogInput::ain4 };

    class Cs43l22PassthroughTest
        : public Cs43l22Test
        , public testing::WithParamInterface<AnalogInput>
    {};

    class Cs43l22RevisionTest
        : public Cs43l22Test
        , public testing::WithParamInterface<uint8_t>
    {};

    int HalfDecibels(uint8_t volumeCode)
    {
        return volumeCode >= 0x34 ? volumeCode - 256 : volumeCode;
    }
}

TEST_F(Cs43l22Test, constructing_holds_the_codec_in_reset_and_does_not_touch_the_bus_or_the_stream)
{
    Create();

    ExecuteAllActions();
}

TEST_F(Cs43l22Test, stereo_between_4_and_100_kHz_is_supported)
{
    for (uint32_t rate : { 4000u, 8000u, 16000u, 44100u, 48000u, 96000u, 100000u })
        EXPECT_TRUE(drivers::Cs43l22::IsSupported({ rate, 2 })) << rate;
}

TEST_F(Cs43l22Test, mono_and_rates_outside_the_range_are_not_supported)
{
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 48000, 1 }));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 48000, 4 }));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 0, 2 }));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 3999, 2 }));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 100001, 2 }));
}

TEST_F(Cs43l22Test, an_unsupported_format_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->Start({ 48000, 1 }, [](hal::AudioOutput::Samples) {}, []() {}), "");
}

TEST_F(Cs43l22Test, the_stream_is_started_and_the_reset_pin_pulled_low_before_anything_else)
{
    Create();

    Start(stereo44k);
}

TEST_F(Cs43l22Test, the_reset_pin_is_released_after_5_ms)
{
    Create();
    Start();

    ForwardTime(Milliseconds(4));

    EXPECT_CALL(reset, Set(true));
    ForwardTime(Milliseconds(1));
}

TEST_F(Cs43l22Test, the_chip_id_is_read_5_ms_after_the_reset_pin_is_released)
{
    Create();
    Start();
    EXPECT_CALL(reset, Set(true));
    ForwardTime(Milliseconds(5));
    ForwardTime(Milliseconds(4));

    testing::InSequence sequence;
    ExpectIdentification();
    ExpectRequiredInitialization();
    ExpectConfiguration();
    ExpectPowerUp();
    ExpectLevel(fullScaleCode, OutputsValue(Output::headphone));
    ForwardTime(Milliseconds(1));
}

TEST_F(Cs43l22Test, a_wrong_chip_id_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(
        {
            EXPECT_CALL(stream, Start(_, _, _));
            EXPECT_CALL(reset, Set(_)).Times(testing::AnyNumber());
            EXPECT_CALL(bus, ReadRegisterMock(0x01)).WillOnce(testing::Return(0x12));
            StartWithoutExpectations(stereo48k);
            ForwardTime(std::chrono::seconds(1));
        },
        "");
}

TEST_P(Cs43l22RevisionTest, any_silicon_revision_is_accepted)
{
    Create();
    Start();
    testing::InSequence sequence;
    EXPECT_CALL(reset, Set(true));
    ExpectIdentification(GetParam());
    ExpectRequiredInitialization();
    ExpectConfiguration();
    ExpectPowerUp();
    ExpectLevel(fullScaleCode, OutputsValue(Output::headphone));

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(Revisions, Cs43l22RevisionTest, testing::Values(uint8_t{ 0xe0 }, uint8_t{ 0xe1 }, uint8_t{ 0xe3 }, uint8_t{ 0xe7 }));

TEST_P(Cs43l22OutputTest, start_up_writes_the_registers_in_order_and_enables_the_outputs_last)
{
    Create(GetParam());
    Start();
    ExpectStartUp(GetParam(), fullScaleCode, false);

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(Outputs, Cs43l22OutputTest, testing::ValuesIn(outputs));

TEST_P(Cs43l22PassthroughTest, the_selected_analog_input_is_routed_to_the_outputs_before_the_power_up)
{
    Create(Output::headphone, 100, GetParam());
    Start();
    ExpectStartUp(Output::headphone, fullScaleCode, false, GetParam());

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(AnalogInputs, Cs43l22PassthroughTest, testing::ValuesIn(analogInputs));

TEST_F(Cs43l22Test, the_initial_volume_is_applied_at_the_end_of_start_up)
{
    Create(Output::headphone, 50);
    Start();

    ExpectStartUp(Output::headphone, drivers::Cs43l22::VolumeRegisterValue(50), false);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, mute_set_before_starting_keeps_the_outputs_off_at_the_end_of_start_up)
{
    Create(Output::speaker);
    SetMuted(true);
    ExecuteAllActions();
    Start();

    ExpectStartUp(Output::speaker, fullScaleCode, true);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, nothing_is_forwarded_to_the_application_during_start_up_and_the_stream_gets_silence)
{
    Create();
    Start();
    ForwardTime(Milliseconds(1));

    OfferPeriod();
    EXPECT_TRUE(BufferIsSilent());
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
}

TEST_F(Cs43l22Test, periods_and_underruns_are_forwarded_once_playing)
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

TEST_F(Cs43l22Test, volume_is_written_to_both_master_volume_registers_while_playing)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(drivers::Cs43l22::VolumeRegisterValue(25), OutputsValue(Output::headphone));

    SetVolume(25);
    ExecuteAllActions();
}

TEST_F(Cs43l22Test, muting_turns_the_outputs_off_and_unmuting_turns_them_back_on_at_the_current_volume)
{
    StartAndWaitUntilPlaying(Output::both, 40);
    const uint8_t code = drivers::Cs43l22::VolumeRegisterValue(40);
    ExpectLevel(code, outputsOff);
    SetMuted(true);
    ExecuteAllActions();

    ExpectLevel(code, OutputsValue(Output::both));
    SetMuted(false);
    ExecuteAllActions();
}

TEST_F(Cs43l22Test, a_change_made_while_a_write_is_in_flight_is_written_afterwards)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectLevel(drivers::Cs43l22::VolumeRegisterValue(10), OutputsValue(Output::headphone));
    ExpectLevel(drivers::Cs43l22::VolumeRegisterValue(10), outputsOff);

    SetVolume(10);
    SetMuted(true);
    ExecuteAllActions();

    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, muteApplied);
}

TEST_F(Cs43l22Test, the_volume_is_reported_once_the_registers_have_been_written)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(drivers::Cs43l22::VolumeRegisterValue(25), OutputsValue(Output::headphone));

    SetVolume(25);
    EXPECT_EQ(0, volumeApplied);

    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
}

TEST_F(Cs43l22Test, stopping_mutes_and_powers_down_first_and_then_pulls_the_reset_pin_low_and_stops_the_stream_after_100_ms)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownWrites();
    Stop();
    ExecuteAllActions();

    ForwardTime(Milliseconds(99));

    ExpectResetAssertedAndStreamStopped();
    ForwardTime(Milliseconds(1));
}

TEST_F(Cs43l22Test, stopping_drops_the_application_callbacks_immediately)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownWrites();

    Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectResetAssertedAndStreamStopped();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, stopping_during_start_up_powers_down_once_start_up_has_finished)
{
    Create();
    Start();
    Stop();

    testing::InSequence sequence;
    ExpectBringUp();
    ExpectShutdownAndStreamStop();
    ForwardTime(std::chrono::seconds(2));
}

TEST_F(Cs43l22Test, starting_again_after_stopping_resets_the_codec_again_with_the_new_format)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownAndStreamStop();
    Stop();
    ForwardTime(std::chrono::seconds(1));

    Start(stereo44k);
    ExpectStartUp(Output::headphone, fullScaleCode, false);
    ForwardTime(std::chrono::seconds(1));

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(Cs43l22Test, starting_while_shutting_down_is_a_programming_error)
{
    StartAndWaitUntilPlaying();
    ExpectPowerDownWrites();
    Stop();

    EXPECT_DEATH(StartWithoutExpectations(stereo44k), "");

    ExpectResetAssertedAndStreamStopped();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, a_volume_above_100_percent_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(SetVolume(101), "");
}

TEST(Cs43l22VolumeTest, zero_percent_is_the_lowest_volume_of_minus_102_dB)
{
    EXPECT_EQ(0x34, drivers::Cs43l22::VolumeRegisterValue(0));
}

TEST(Cs43l22VolumeTest, one_hundred_percent_is_0_dB)
{
    EXPECT_EQ(0x00, drivers::Cs43l22::VolumeRegisterValue(100));
}

TEST(Cs43l22VolumeTest, fifty_percent_is_minus_51_dB)
{
    EXPECT_EQ(0x9a, drivers::Cs43l22::VolumeRegisterValue(50));
}

TEST(Cs43l22VolumeTest, the_volume_never_decreases_with_the_percentage)
{
    for (uint8_t percent = 1; percent <= 100; ++percent)
        EXPECT_GE(HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(percent)), HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(static_cast<uint8_t>(percent - 1)))) << int(percent);
}

TEST(Cs43l22VolumeTest, a_percentage_above_100_is_a_programming_error)
{
    EXPECT_DEATH(drivers::Cs43l22::VolumeRegisterValue(101), "");
}

TEST(Cs43l22VolumeTest, the_volume_never_exceeds_0_dB)
{
    for (uint8_t percent = 0; percent <= 100; ++percent)
        EXPECT_LE(HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(percent)), 0) << int(percent);
}

TEST_F(Cs43l22Test, destroying_while_playing_stops_the_stream_and_releases_the_reset_pin)
{
    StartAndWaitUntilPlaying();

    EXPECT_CALL(stream, Stop(_));
    EXPECT_CALL(reset, ResetConfig());
    codec.reset();
}

TEST_F(Cs43l22Test, destroying_while_idle_leaves_the_stream_alone)
{
    Create();

    codec.reset();
}

TEST_F(Cs43l22Test, destroying_while_waiting_for_a_delay_stops_the_stream_and_cancels_the_wait)
{
    Create();
    Start();

    EXPECT_CALL(stream, Stop(_));
    codec.reset();

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, stopping_with_a_completion_reports_done_once_the_codec_is_powered_down_and_the_stream_stopped)
{
    StartAndWaitUntilPlaying();
    testing::StrictMock<infra::MockCallback<void()>> stopped;
    testing::InSequence sequence;
    ExpectShutdownAndStreamStop();
    EXPECT_CALL(stopped, callback());

    codec->Stop([&stopped]()
        {
            stopped.callback();
        });
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, the_codec_may_be_destroyed_from_the_completion)
{
    StartAndWaitUntilPlaying();
    ExpectShutdownAndStreamStop();

    codec->Stop([this]()
        {
            codec.reset();
        });
    ForwardTime(std::chrono::seconds(1));

    EXPECT_FALSE(codec.has_value());
}
