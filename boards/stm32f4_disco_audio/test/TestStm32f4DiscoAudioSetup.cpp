#include "boards/stm32f4_disco_audio/Stm32f4DiscoAudioSetup.hpp"
#include "hal/interfaces/I2c.hpp"
#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "hal/interfaces/test_doubles/GpioMock.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/timer/test_helper/ClockFixture.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace
{
    using testing::_;

    constexpr hal::I2cAddress codecAddress{ 0x4a };
    constexpr hal::AudioFormat stereo48k{ 48000, 2 };
    constexpr hal::AudioFormat stereo96k{ 96000, 2 };
    constexpr hal::AudioFormat stereo16k{ 16000, 2 };
    constexpr uint8_t chipId = 0xe3;
    constexpr uint8_t chipIdRegister = 0x01;

    class I2cRecorder
        : public hal::I2cMaster
    {
    public:
        struct Access
        {
            bool addressedToTheCodec;
            bool write;
            uint8_t dataRegister;
            uint8_t value;
        };

        void SendData(hal::I2cAddress address, infra::ConstByteRange data, hal::Action nextAction, infra::Function<void(hal::Result, uint32_t numberOfBytesSent)> onSent) override
        {
            ASSERT_EQ(std::size_t(1), data.size());

            if (nextAction == hal::Action::stop)
                Record({ address == codecAddress, true, selectedRegister, data[0] });
            else
                selectedRegister = data[0];

            sent = onSent;
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    auto done = sent;
                    done(hal::Result::complete, 1);
                });
        }

        void ReceiveData(hal::I2cAddress address, infra::ByteRange data, hal::Action nextAction, infra::Function<void(hal::Result)> onReceived) override
        {
            ASSERT_EQ(std::size_t(1), data.size());

            data[0] = selectedRegister == chipIdRegister ? chipId : uint8_t{ 0 };
            Record({ address == codecAddress, false, selectedRegister, data[0] });

            received = onReceived;
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    auto done = received;
                    done(hal::Result::complete);
                });
        }

        bool Wrote(uint8_t dataRegister, uint8_t value) const
        {
            return IndexOfWrite(dataRegister, value).has_value();
        }

        bool Wrote(uint8_t dataRegister) const
        {
            for (std::size_t i = 0; i != count; ++i)
                if (log[i].write && log[i].dataRegister == dataRegister)
                    return true;

            return false;
        }

        std::optional<std::size_t> IndexOfWrite(uint8_t dataRegister, uint8_t value) const
        {
            for (std::size_t i = 0; i != count; ++i)
                if (log[i].write && log[i].dataRegister == dataRegister && log[i].value == value)
                    return i;

            return std::nullopt;
        }

        bool EveryAccessIsAddressedToTheCodec() const
        {
            for (std::size_t i = 0; i != count; ++i)
                if (!log[i].addressedToTheCodec)
                    return false;

            return count != 0;
        }

    private:
        void Record(const Access& access)
        {
            ASSERT_LT(count, log.size());
            log[count++] = access;
        }

        std::array<Access, 64> log{};
        std::size_t count{ 0 };
        uint8_t selectedRegister{ 0 };
        infra::Function<void(hal::Result, uint32_t)> sent;
        infra::Function<void(hal::Result)> received;
    };

    class Stm32f4DiscoAudioSetupTest
        : public testing::Test
        , public infra::ClockFixture
    {
    public:
        Stm32f4DiscoAudioSetupTest()
        {
            EXPECT_CALL(stream, Stop(_)).Times(testing::AtMost(1));
            EXPECT_CALL(reset, ResetConfig()).Times(testing::AtMost(1));
        }

        void Create(uint8_t initialVolume = boards::Stm32f4DiscoAudioSetup::defaultInitialVolume)
        {
            EXPECT_CALL(reset, Config(hal::PinConfigType::output, false));
            board.emplace(i2c, reset, stream, initialVolume);
        }

        void StartWithoutExpectations(hal::AudioFormat format)
        {
            board->Output().Start(
                format, [](hal::AudioOutput::Samples) {}, []() {});
        }

        void Start(hal::AudioFormat format = stereo48k)
        {
            EXPECT_CALL(stream, Start(format, _, _));
            EXPECT_CALL(reset, Set(_)).Times(testing::AnyNumber());
            StartWithoutExpectations(format);
            ForwardTime(std::chrono::seconds(1));
        }

        I2cRecorder i2c;
        testing::StrictMock<hal::AudioOutputMock> stream;
        testing::StrictMock<hal::GpioPinMock> reset;
        std::optional<boards::Stm32f4DiscoAudioSetup> board;
    };
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_stream_is_started_before_the_codec_is_taken_out_of_reset)
{
    Create();
    testing::InSequence sequence;
    EXPECT_CALL(stream, Start(stereo48k, _, _));
    EXPECT_CALL(reset, Set(false));
    EXPECT_CALL(reset, Set(true));

    StartWithoutExpectations(stereo48k);
    ForwardTime(std::chrono::seconds(1));
}

TEST_F(Stm32f4DiscoAudioSetupTest, every_access_is_addressed_to_the_codec_with_the_ad0_pin_low)
{
    Create();

    Start();

    EXPECT_TRUE(i2c.EveryAccessIsAddressedToTheCodec());
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_headphone_output_is_enabled_and_the_codec_is_a_slave_with_an_i2s_stream)
{
    Create();

    Start();

    EXPECT_TRUE(i2c.Wrote(0x04, 0xaf));
    EXPECT_TRUE(i2c.Wrote(0x06, 0x04));
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_codec_is_powered_up_after_it_has_been_configured)
{
    Create();

    Start();

    ASSERT_TRUE(i2c.IndexOfWrite(0x02, 0x9e).has_value());
    EXPECT_LT(*i2c.IndexOfWrite(0x06, 0x04), *i2c.IndexOfWrite(0x02, 0x9e));
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_master_clock_of_256_times_the_sample_rate_is_not_divided_at_48_kHz)
{
    Create();

    Start(stereo48k);

    EXPECT_TRUE(i2c.Wrote(0x05, 0xa0));
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_master_clock_of_256_times_the_sample_rate_is_divided_by_2_at_96_kHz)
{
    Create();

    Start(stereo96k);

    EXPECT_TRUE(i2c.Wrote(0x05, 0xa1));
}

TEST_F(Stm32f4DiscoAudioSetupTest, a_sample_rate_below_32_kHz_needs_a_larger_master_clock_and_is_a_programming_error)
{
    Create();

    EXPECT_DEATH(StartWithoutExpectations(stereo16k), "");
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_default_initial_volume_is_applied)
{
    Create();

    Start();

    EXPECT_TRUE(i2c.Wrote(0x20, drivers::Cs43l22::VolumeRegisterValue(boards::Stm32f4DiscoAudioSetup::defaultInitialVolume)));
    EXPECT_TRUE(i2c.Wrote(0x21, drivers::Cs43l22::VolumeRegisterValue(boards::Stm32f4DiscoAudioSetup::defaultInitialVolume)));
}

TEST_F(Stm32f4DiscoAudioSetupTest, a_given_initial_volume_is_applied)
{
    Create(80);

    Start();

    EXPECT_TRUE(i2c.Wrote(0x20, drivers::Cs43l22::VolumeRegisterValue(80)));
}

TEST_F(Stm32f4DiscoAudioSetupTest, no_analog_input_is_routed_to_the_outputs)
{
    Create();

    Start();

    EXPECT_FALSE(i2c.Wrote(0x08));
    EXPECT_FALSE(i2c.Wrote(0x09));
    EXPECT_FALSE(i2c.Wrote(0x14));
    EXPECT_FALSE(i2c.Wrote(0x15));
}

TEST_F(Stm32f4DiscoAudioSetupTest, the_volume_is_set_through_the_output_of_the_board)
{
    Create();
    Start();
    bool applied = false;

    board->Output().SetVolume(20, [&applied]()
        {
            applied = true;
        });
    ExecuteAllActions();

    EXPECT_TRUE(applied);
    EXPECT_TRUE(i2c.Wrote(0x20, drivers::Cs43l22::VolumeRegisterValue(20)));
}
