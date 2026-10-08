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
    constexpr hal::AudioFormat stereo96k{ 96000, 2 };
    constexpr uint8_t chipIdRevisionB1 = 0xe3;
    constexpr uint16_t ratio256 = 256;
    constexpr uint8_t clockingAuto = 0xa0;
    constexpr uint8_t clockingAutoDivideBy2 = 0xa1;
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
            ResetRegisters();
        }

        void ResetRegisters()
        {
            registers.fill(0);
            registers[0x08] = 0x81;
            registers[0x09] = 0x81;
            registers[0x0a] = 0x95;
            registers[0x0d] = 0x60;
            registers[0x0e] = 0x02;
            registers[0x32] = 0x3b;
        }

        void Create(Output output = Output::headphone, uint8_t initialVolume = 100, std::optional<AnalogInput> passthrough = std::nullopt, uint16_t masterClockRatio = ratio256)
        {
            configuredPassthrough = passthrough;
            configuredOutput = output;

            EXPECT_CALL(reset, Config(hal::PinConfigType::output, false));
            codec.emplace(bus, stream, reset, drivers::Cs43l22::Config{ output, initialVolume, masterClockRatio, passthrough });
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
            ExpectStartUp(volume, false);
            ForwardTime(std::chrono::seconds(1));
        }

        void ExpectWrite(uint8_t address, uint8_t value)
        {
            EXPECT_CALL(bus, WriteRegisterMock(address, value));
            registers[address] = value;
        }

        void ExpectModify(uint8_t address, uint8_t clearMask, uint8_t setMask)
        {
            const uint8_t before = registers[address];
            const uint8_t after = static_cast<uint8_t>((before & ~clearMask) | setMask);

            testing::InSequence sequence;
            EXPECT_CALL(bus, ReadRegisterMock(address)).WillOnce(testing::Return(before));
            EXPECT_CALL(bus, WriteRegisterMock(address, after));
            registers[address] = after;
        }

        void ExpectIdentification(uint8_t chipId = chipIdRevisionB1)
        {
            EXPECT_CALL(bus, ReadRegisterMock(0x01)).WillOnce(testing::Return(chipId));
        }

        void ExpectConfiguration(uint8_t clocking = clockingAuto)
        {
            testing::InSequence sequence;
            ExpectWrite(0x04, OutputsValue(configuredOutput));
            ExpectWrite(0x05, clocking);
            ExpectWrite(0x06, 0x04);
            ExpectModify(0x0d, 0x00, 0x03);
        }

        void ExpectPassthroughRouting(AnalogInput input)
        {
            const uint8_t select = static_cast<uint8_t>(1 << static_cast<uint8_t>(input));

            testing::InSequence sequence;
            ExpectModify(0x08, 0x0f, select);
            ExpectModify(0x09, 0x0f, select);
            ExpectModify(0x0e, 0x00, 0xf0);
        }

        void ExpectRequiredInitialization()
        {
            testing::InSequence sequence;
            ExpectWrite(0x00, 0x99);
            ExpectWrite(0x47, 0x80);
            ExpectModify(0x32, 0x00, 0x80);
            ExpectModify(0x32, 0x80, 0x00);
            ExpectWrite(0x00, 0x00);
        }

        void ExpectPowerUp()
        {
            ExpectWrite(0x02, 0x9e);
        }

        void ExpectBringUp(uint8_t clocking = clockingAuto)
        {
            ResetRegisters();

            testing::InSequence sequence;
            EXPECT_CALL(reset, Set(true));
            ExpectIdentification();
            ExpectConfiguration(clocking);
            if (configuredPassthrough)
                ExpectPassthroughRouting(*configuredPassthrough);
            ExpectRequiredInitialization();
            ExpectPowerUp();
        }

        void ExpectLevel(uint8_t percent, bool isMuted)
        {
            testing::InSequence sequence;
            const uint8_t volumeCode = drivers::Cs43l22::VolumeRegisterValue(percent);
            ExpectWrite(0x20, volumeCode);
            ExpectWrite(0x21, volumeCode);
            ExpectModify(0x0d, 0x03, isMuted ? 0x03 : 0x00);

            if (configuredPassthrough)
            {
                const uint8_t passthroughCode = drivers::Cs43l22::PassthroughVolumeRegisterValue(percent);
                ExpectWrite(0x14, passthroughCode);
                ExpectWrite(0x15, passthroughCode);
                ExpectModify(0x0e, 0x30, isMuted ? 0x30 : 0x00);
            }
        }

        void ExpectStartUp(uint8_t percent, bool isMuted, uint8_t clocking = clockingAuto)
        {
            testing::InSequence sequence;
            ExpectBringUp(clocking);
            ExpectLevel(percent, isMuted);
        }

        void ExpectMute()
        {
            testing::InSequence sequence;
            ExpectModify(0x0d, 0x00, 0x03);
            if (configuredPassthrough)
                ExpectModify(0x0e, 0x00, 0x30);
        }

        void ExpectPowerDownWrites()
        {
            testing::InSequence sequence;
            ExpectModify(0x0e, 0x03, 0x00);
            ExpectModify(0x0a, 0x0f, 0x00);
            ExpectWrite(0x02, 0x9f);
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
            ExpectMute();
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
        std::array<uint8_t, 256> registers{};
        Output configuredOutput{ Output::headphone };
        std::optional<AnalogInput> configuredPassthrough;
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

    struct ClockCase
    {
        uint32_t sampleRate;
        uint16_t masterClockRatio;
    };

    const std::array<ClockCase, 18> supportedClocks{ {
        { 8000, 1024 },
        { 8000, 1536 },
        { 8000, 2048 },
        { 8000, 3072 },
        { 16000, 512 },
        { 16000, 768 },
        { 16000, 1024 },
        { 16000, 1536 },
        { 32000, 256 },
        { 44100, 256 },
        { 48000, 384 },
        { 48000, 512 },
        { 48000, 768 },
        { 64000, 128 },
        { 96000, 128 },
        { 96000, 192 },
        { 96000, 256 },
        { 96000, 384 },
    } };

    int HalfDecibels(uint8_t volumeCode, uint8_t firstNegativeCode)
    {
        return volumeCode >= firstNegativeCode ? volumeCode - 256 : volumeCode;
    }
}

TEST_F(Cs43l22Test, constructing_holds_the_codec_in_reset_and_does_not_touch_the_bus_or_the_stream)
{
    Create();

    ExecuteAllActions();
}

TEST_F(Cs43l22Test, stereo_between_4_and_96_kHz_is_supported_with_the_master_clock_ratios_of_its_speed_mode)
{
    for (const ClockCase& clock : supportedClocks)
        EXPECT_TRUE(drivers::Cs43l22::IsSupported({ clock.sampleRate, 2 }, clock.masterClockRatio, Output::headphone)) << clock.sampleRate << " " << clock.masterClockRatio;
}

TEST_F(Cs43l22Test, mono_and_rates_outside_the_range_are_not_supported)
{
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 48000, 1 }, 256, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 48000, 4 }, 256, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 0, 2 }, 256, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 3999, 2 }, 1024, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 96001, 2 }, 128, Output::headphone));
}

TEST_F(Cs43l22Test, a_master_clock_ratio_that_does_not_fit_the_sample_rate_is_not_supported)
{
    EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo48k, 128, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo48k, 300, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 16000, 2 }, 256, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 8000, 2 }, 512, Output::headphone));
    EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo96k, 512, Output::headphone));
}

TEST_F(Cs43l22Test, the_speaker_amplifiers_are_not_supported_with_a_master_clock_of_16_9344_or_18_432_MHz)
{
    for (Output output : { Output::speaker, Output::both, Output::automatic })
    {
        EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo48k, 384, output));
        EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo44k, 384, output));
        EXPECT_FALSE(drivers::Cs43l22::IsSupported(stereo96k, 192, output));
        EXPECT_FALSE(drivers::Cs43l22::IsSupported({ 24000, 2 }, 768, output));
    }

    EXPECT_TRUE(drivers::Cs43l22::IsSupported(stereo48k, 384, Output::headphone));
    EXPECT_TRUE(drivers::Cs43l22::IsSupported(stereo48k, 256, Output::speaker));
}

TEST_F(Cs43l22Test, an_unsupported_format_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(codec->Start({ 48000, 1 }, [](hal::AudioOutput::Samples) {}, []() {}), "");
}

TEST_F(Cs43l22Test, a_master_clock_ratio_that_does_not_fit_the_sample_rate_is_a_programming_error)
{
    Create(Output::headphone, 100, std::nullopt, 300);

    EXPECT_DEATH(codec->Start(stereo48k, [](hal::AudioOutput::Samples) {}, []() {}), "");
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
    ExpectConfiguration();
    ExpectRequiredInitialization();
    ExpectPowerUp();
    ExpectLevel(100, false);
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
    ExpectConfiguration();
    ExpectRequiredInitialization();
    ExpectPowerUp();
    ExpectLevel(100, false);

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(Revisions, Cs43l22RevisionTest, testing::Values(uint8_t{ 0xe0 }, uint8_t{ 0xe1 }, uint8_t{ 0xe2 }, uint8_t{ 0xe3 }));

TEST_P(Cs43l22OutputTest, start_up_configures_before_the_required_initialization_and_powers_up_last)
{
    Create(GetParam());
    Start();
    ExpectStartUp(100, false);

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(Outputs, Cs43l22OutputTest, testing::ValuesIn(outputs));

TEST_P(Cs43l22PassthroughTest, the_selected_analog_input_is_routed_and_muted_before_the_power_up)
{
    Create(Output::headphone, 100, GetParam());
    Start();
    ExpectStartUp(100, false);

    ForwardTime(std::chrono::seconds(1));
}

INSTANTIATE_TEST_SUITE_P(AnalogInputs, Cs43l22PassthroughTest, testing::ValuesIn(analogInputs));

TEST_F(Cs43l22Test, the_passthrough_select_registers_keep_their_reserved_bit_set)
{
    Create(Output::headphone, 100, AnalogInput::ain3);
    Start();
    ExpectStartUp(100, false);

    EXPECT_EQ(0x84, registers[0x08]);
    EXPECT_EQ(0x84, registers[0x09]);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, the_master_clock_is_not_divided_when_the_ratio_is_the_base_ratio_of_the_sample_rate)
{
    Create(Output::headphone, 100, std::nullopt, 256);
    Start(stereo44k);
    ExpectStartUp(100, false, clockingAuto);

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, the_master_clock_is_divided_by_2_when_the_ratio_is_twice_the_base_ratio_of_the_sample_rate)
{
    Create(Output::headphone, 100, std::nullopt, 256);
    Start(stereo96k);
    ExpectStartUp(100, false, clockingAutoDivideBy2);

    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, the_initial_volume_is_applied_at_the_end_of_start_up)
{
    Create(Output::headphone, 50);
    Start();

    ExpectStartUp(50, false);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, mute_set_before_starting_is_applied_at_the_end_of_start_up)
{
    Create(Output::speaker);
    SetMuted(true);
    ExecuteAllActions();
    Start();

    ExpectStartUp(100, true);
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
    ExpectLevel(25, false);

    SetVolume(25);
    ExecuteAllActions();
}

TEST_F(Cs43l22Test, muting_mutes_the_master_volume_and_unmuting_restores_it_at_the_current_volume)
{
    StartAndWaitUntilPlaying(Output::both, 40);
    ExpectLevel(40, true);
    SetMuted(true);
    ExecuteAllActions();

    ExpectLevel(40, false);
    SetMuted(false);
    ExecuteAllActions();
}

TEST_F(Cs43l22Test, the_passthrough_follows_the_volume_and_is_muted_with_the_output)
{
    StartAndWaitUntilPlaying(Output::headphone, 100, AnalogInput::ain2);
    ExpectLevel(30, false);
    SetVolume(30);
    ExecuteAllActions();

    ExpectLevel(30, true);
    SetMuted(true);
    ExecuteAllActions();
}

TEST_F(Cs43l22Test, a_change_made_while_a_write_is_in_flight_is_written_afterwards)
{
    StartAndWaitUntilPlaying();
    testing::InSequence sequence;
    ExpectLevel(10, false);
    ExpectLevel(10, true);

    SetVolume(10);
    SetMuted(true);
    ExecuteAllActions();

    EXPECT_EQ(1, volumeApplied);
    EXPECT_EQ(1, muteApplied);
}

TEST_F(Cs43l22Test, the_volume_is_reported_once_the_registers_have_been_written)
{
    StartAndWaitUntilPlaying();
    ExpectLevel(25, false);

    SetVolume(25);
    EXPECT_EQ(0, volumeApplied);

    ExecuteAllActions();
    EXPECT_EQ(1, volumeApplied);
}

TEST_F(Cs43l22Test, stopping_mutes_waits_100_ms_for_the_ramp_powers_down_waits_1_ms_and_then_pulls_the_reset_pin_low_and_stops_the_stream)
{
    StartAndWaitUntilPlaying();
    ExpectMute();
    Stop();
    ExecuteAllActions();

    ForwardTime(Milliseconds(99));

    ExpectPowerDownWrites();
    ForwardTime(Milliseconds(1));

    ForwardTime(std::chrono::microseconds(900));

    ExpectResetAssertedAndStreamStopped();
    ForwardTime(std::chrono::microseconds(100));
}

TEST_F(Cs43l22Test, stopping_with_the_passthrough_mutes_the_passthrough_too)
{
    StartAndWaitUntilPlaying(Output::headphone, 100, AnalogInput::ain1);
    ExpectShutdownAndStreamStop();

    Stop();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, stopping_drops_the_application_callbacks_immediately)
{
    StartAndWaitUntilPlaying();
    ExpectMute();

    Stop();
    OfferPeriod();
    transportUnderrun();

    EXPECT_EQ(0, periods);
    EXPECT_EQ(0, underruns);
    EXPECT_TRUE(BufferIsSilent());
    ExpectPowerDownWrites();
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
    ExpectStartUp(100, false);
    ForwardTime(std::chrono::seconds(1));

    OfferPeriod();
    EXPECT_EQ(1, periods);
}

TEST_F(Cs43l22Test, starting_while_shutting_down_is_a_programming_error)
{
    StartAndWaitUntilPlaying();
    ExpectMute();
    Stop();

    EXPECT_DEATH(StartWithoutExpectations(stereo44k), "");

    ExpectPowerDownWrites();
    ExpectResetAssertedAndStreamStopped();
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Cs43l22Test, a_volume_above_100_percent_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(SetVolume(101), "");
}

TEST(Cs43l22VolumeTest, zero_percent_is_the_lowest_master_volume_of_minus_102_dB)
{
    EXPECT_EQ(0x34, drivers::Cs43l22::VolumeRegisterValue(0));
}

TEST(Cs43l22VolumeTest, one_hundred_percent_is_a_master_volume_of_0_dB)
{
    EXPECT_EQ(0x00, drivers::Cs43l22::VolumeRegisterValue(100));
}

TEST(Cs43l22VolumeTest, fifty_percent_is_a_master_volume_of_minus_51_dB)
{
    EXPECT_EQ(0x9a, drivers::Cs43l22::VolumeRegisterValue(50));
}

TEST(Cs43l22VolumeTest, the_master_volume_never_decreases_with_the_percentage)
{
    for (uint8_t percent = 1; percent <= 100; ++percent)
        EXPECT_GE(HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(percent), 0x34), HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(static_cast<uint8_t>(percent - 1)), 0x34)) << int(percent);
}

TEST(Cs43l22VolumeTest, the_master_volume_never_exceeds_0_dB)
{
    for (uint8_t percent = 0; percent <= 100; ++percent)
        EXPECT_LE(HalfDecibels(drivers::Cs43l22::VolumeRegisterValue(percent), 0x34), 0) << int(percent);
}

TEST(Cs43l22VolumeTest, zero_percent_is_the_lowest_passthrough_volume_of_minus_60_dB)
{
    EXPECT_EQ(0x88, drivers::Cs43l22::PassthroughVolumeRegisterValue(0));
}

TEST(Cs43l22VolumeTest, one_hundred_percent_is_a_passthrough_volume_of_0_dB)
{
    EXPECT_EQ(0x00, drivers::Cs43l22::PassthroughVolumeRegisterValue(100));
}

TEST(Cs43l22VolumeTest, fifty_percent_is_a_passthrough_volume_of_minus_30_dB)
{
    EXPECT_EQ(0xc4, drivers::Cs43l22::PassthroughVolumeRegisterValue(50));
}

TEST(Cs43l22VolumeTest, the_passthrough_volume_never_decreases_with_the_percentage_and_never_exceeds_0_dB)
{
    for (uint8_t percent = 1; percent <= 100; ++percent)
    {
        EXPECT_GE(HalfDecibels(drivers::Cs43l22::PassthroughVolumeRegisterValue(percent), 0x88), HalfDecibels(drivers::Cs43l22::PassthroughVolumeRegisterValue(static_cast<uint8_t>(percent - 1)), 0x88)) << int(percent);
        EXPECT_LE(HalfDecibels(drivers::Cs43l22::PassthroughVolumeRegisterValue(percent), 0x88), 0) << int(percent);
    }
}

TEST(Cs43l22VolumeTest, a_percentage_above_100_is_a_programming_error)
{
    EXPECT_DEATH(drivers::Cs43l22::VolumeRegisterValue(101), "");
    EXPECT_DEATH(drivers::Cs43l22::PassthroughVolumeRegisterValue(101), "");
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
    testing::StrictMock<infra::MockCallback<void()>> done;
    testing::InSequence sequence;
    ExpectShutdownAndStreamStop();
    EXPECT_CALL(done, callback());

    codec->Stop([&done]()
        {
            done.callback();
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
