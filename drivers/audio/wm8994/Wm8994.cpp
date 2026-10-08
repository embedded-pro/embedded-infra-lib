#include "drivers/audio/wm8994/Wm8994.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>
#include <chrono>
#include <optional>

namespace drivers
{
    namespace
    {
        constexpr uint16_t chipIdRegister = 0x0000;
        constexpr uint16_t softwareResetRegister = 0x0000;
        constexpr uint16_t powerManagement1Register = 0x0001;
        constexpr uint16_t powerManagement3Register = 0x0003;
        constexpr uint16_t powerManagement5Register = 0x0005;
        constexpr uint16_t speakerMixerLeftAttenuationRegister = 0x0022;
        constexpr uint16_t speakerMixerRightAttenuationRegister = 0x0023;
        constexpr uint16_t outputMixer1Register = 0x002d;
        constexpr uint16_t outputMixer2Register = 0x002e;
        constexpr uint16_t speakerMixerRegister = 0x0036;
        constexpr uint16_t antiPop2Register = 0x0039;
        constexpr uint16_t erratumRegister56 = 0x0056;
        constexpr uint16_t testKeyRegister = 0x0102;
        constexpr uint16_t writeSequencerControlRegister = 0x0110;
        constexpr uint16_t erratumRegister817 = 0x0817;
        constexpr uint16_t aif1Clocking1Register = 0x0200;
        constexpr uint16_t clocking1Register = 0x0208;
        constexpr uint16_t aif1RateRegister = 0x0210;
        constexpr uint16_t aif1Control1Register = 0x0300;
        constexpr uint16_t aif1Dac1Filters1Register = 0x0420;
        constexpr uint16_t dac1LeftMixerRoutingRegister = 0x0601;
        constexpr uint16_t dac1RightMixerRoutingRegister = 0x0602;
        constexpr uint16_t dac1LeftVolumeRegister = 0x0610;
        constexpr uint16_t dac1RightVolumeRegister = 0x0611;

        constexpr uint16_t expectedChipId = 0x8994;

        constexpr uint16_t patchUnlock = 0x0003;
        constexpr uint16_t patchLock = 0x0000;
        constexpr uint16_t erratumValue56 = 0x0003;
        constexpr uint16_t erratumValue817 = 0x0000;

        constexpr uint16_t vmidRampFast = 0x0060;
        constexpr uint16_t vmidBufferEnable = 0x0008;
        constexpr uint16_t startupBiasEnable = 0x0004;
        constexpr uint16_t antiPop2Value = vmidRampFast | vmidBufferEnable | startupBiasEnable;

        constexpr uint16_t biasEnable = 0x0001;
        constexpr uint16_t vmidSelect = 0x0002;
        constexpr uint16_t biasAndVmid = biasEnable | vmidSelect;
        constexpr uint16_t speakerOutputsEnable = 0x3000;

        constexpr uint16_t aif1Dac1LeftEnable = 0x0200;
        constexpr uint16_t aif1Dac1RightEnable = 0x0100;
        constexpr uint16_t dac1LeftEnable = 0x0002;
        constexpr uint16_t dac1RightEnable = 0x0001;
        constexpr uint16_t aif1Dac1AndDac1Enable = aif1Dac1LeftEnable | aif1Dac1RightEnable | dac1LeftEnable | dac1RightEnable;
        constexpr uint16_t aif1Dac1ToDac1 = 0x0001;

        constexpr uint16_t aif1AdcrSource = 0x4000;
        constexpr uint16_t aif1FormatI2s = 0x0010;
        constexpr uint16_t aif1WordLength16Bit = 0x0000;
        constexpr uint16_t aif1ControlI2s16Bit = aif1AdcrSource | aif1FormatI2s | aif1WordLength16Bit;
        constexpr uint16_t systemDspClockEnable = 0x0002;
        constexpr uint16_t aif1DspClockEnable = 0x0008;
        constexpr uint16_t dspClocksEnable = systemDspClockEnable | aif1DspClockEnable;
        constexpr uint16_t aif1ClockEnable = 0x0001;

        constexpr uint16_t dac1ToHeadphoneOutput = 0x0100;
        constexpr uint16_t writeSequencerEnable = 0x8000;
        constexpr uint16_t writeSequencerStart = 0x0100;
        constexpr uint16_t headphoneColdStartUpIndex = 0x0000;
        constexpr uint16_t headphoneColdStartUp = writeSequencerEnable | writeSequencerStart | headphoneColdStartUpIndex;

        constexpr uint16_t speakerLeftVolumeEnable = 0x0100;
        constexpr uint16_t speakerRightVolumeEnable = 0x0200;
        constexpr uint16_t speakerVolumesEnable = speakerLeftVolumeEnable | speakerRightVolumeEnable;
        constexpr uint16_t speakerMixerAttenuation0dB = 0x0000;
        constexpr uint16_t dac1ToSpeakerMixerLeft = 0x0002;
        constexpr uint16_t dac1ToSpeakerMixerRight = 0x0001;
        constexpr uint16_t dac1ToSpeakerMixers = dac1ToSpeakerMixerLeft | dac1ToSpeakerMixerRight;

        constexpr uint16_t volumeUpdate = 0x0100;
        constexpr uint16_t softMute = 0x0200;
        constexpr uint16_t unmuteWithRamp = 0x0010;
        constexpr uint16_t allOff = 0x0000;

        constexpr uint16_t biasSettleInMilliseconds = 50;
        constexpr uint16_t headphoneColdStartUpInMilliseconds = 325;
        constexpr uint16_t muteRampInMilliseconds = 100;

        constexpr int fullScaleVolumeCode = 0xc0;
        constexpr uint16_t mclkRatio256 = 0x0003;
        constexpr uint16_t sampleRateShift = 4;
        constexpr uint8_t supportedChannels = 2;

        constexpr std::array<uint32_t, 11> sampleRates{ 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000 };

        constexpr std::array<Wm8994::Step, 6> bringUpSteps{ {
            { testKeyRegister, patchUnlock, 0 },
            { erratumRegister56, erratumValue56, 0 },
            { erratumRegister817, erratumValue817, 0 },
            { testKeyRegister, patchLock, 0 },
            { antiPop2Register, antiPop2Value, 0 },
            { powerManagement1Register, biasAndVmid, biasSettleInMilliseconds },
        } };

        constexpr std::array<Wm8994::Step, 3> pathSteps{ {
            { powerManagement5Register, aif1Dac1AndDac1Enable, 0 },
            { dac1LeftMixerRoutingRegister, aif1Dac1ToDac1, 0 },
            { dac1RightMixerRoutingRegister, aif1Dac1ToDac1, 0 },
        } };

        constexpr std::array<Wm8994::Step, 3> clockingSteps{ {
            { aif1Control1Register, aif1ControlI2s16Bit, 0 },
            { clocking1Register, dspClocksEnable, 0 },
            { aif1Clocking1Register, aif1ClockEnable, 0 },
        } };

        constexpr std::array<Wm8994::Step, 3> headphoneSteps{ {
            { outputMixer1Register, dac1ToHeadphoneOutput, 0 },
            { outputMixer2Register, dac1ToHeadphoneOutput, 0 },
            { writeSequencerControlRegister, headphoneColdStartUp, headphoneColdStartUpInMilliseconds },
        } };

        constexpr std::array<Wm8994::Step, 5> speakerSteps{ {
            { powerManagement3Register, speakerVolumesEnable, 0 },
            { speakerMixerLeftAttenuationRegister, speakerMixerAttenuation0dB, 0 },
            { speakerMixerRightAttenuationRegister, speakerMixerAttenuation0dB, 0 },
            { speakerMixerRegister, dac1ToSpeakerMixers, 0 },
            { powerManagement1Register, speakerOutputsEnable | biasAndVmid, 0 },
        } };

        constexpr std::array<Wm8994::Step, 5> shutDownSteps{ {
            { aif1Dac1Filters1Register, softMute, muteRampInMilliseconds },
            { outputMixer1Register, allOff, 0 },
            { outputMixer2Register, allOff, 0 },
            { powerManagement5Register, allOff, 0 },
            { softwareResetRegister, allOff, 0 },
        } };

        std::optional<uint16_t> Aif1RateValue(uint32_t sampleRate)
        {
            auto position = std::find(sampleRates.begin(), sampleRates.end(), sampleRate);

            if (position == sampleRates.end())
                return std::nullopt;

            return static_cast<uint16_t>((position - sampleRates.begin()) << sampleRateShift | mclkRatio256);
        }
    }

    Wm8994::Wm8994(services::RegisterBusAccessHalfWord& bus, hal::AudioOutput& stream, const Config& config)
        : CodecAudioOutput(stream, config.initialVolume)
        , bus(bus)
        , config(config)
    {}

    Wm8994::~Wm8994()
    {
        really_assert(!Busy() || timer.Armed());
    }

    bool Wm8994::IsSupported(hal::AudioFormat format)
    {
        return format.channels == supportedChannels && Aif1RateValue(format.sampleRate).has_value();
    }

    uint8_t Wm8994::VolumeRegisterValue(uint8_t percent)
    {
        really_assert(percent <= maxVolumePercent);

        if (percent == 0)
            return 0;

        return static_cast<uint8_t>(std::max(1, (percent * fullScaleVolumeCode + maxVolumePercent / 2) / maxVolumePercent));
    }

    bool Wm8994::Supports(hal::AudioFormat format) const
    {
        return IsSupported(format);
    }

    void Wm8994::BeginBringUp(hal::AudioFormat format)
    {
        rateRegisterValue = *Aif1RateValue(format.sampleRate);

        chipIdBytes = {};
        bus.ReadRegister(chipIdRegister, infra::MakeByteRange(chipIdBytes), [this]()
            {
                ChipIdRead();
            });
    }

    void Wm8994::ChipIdRead()
    {
        really_assert(((chipIdBytes[0] << 8) | chipIdBytes[1]) == expectedChipId);

        BuildStartUp();
        RunSequence();
    }

    void Wm8994::BuildStartUp()
    {
        sequence.clear();
        Append(infra::MakeRange(bringUpSteps));
        Append(infra::MakeRange(pathSteps));
        sequence.push_back({ aif1RateRegister, rateRegisterValue, 0 });
        Append(infra::MakeRange(clockingSteps));
        Append(config.output == Output::headphone ? infra::MakeRange(headphoneSteps) : infra::MakeRange(speakerSteps));
    }

    void Wm8994::Append(infra::MemoryRange<const Step> steps)
    {
        for (const Step& step : steps)
            sequence.push_back(step);
    }

    void Wm8994::BeginApplyLevel(uint8_t volumePercent, bool muted)
    {
        const uint16_t volumeCode = static_cast<uint16_t>(volumeUpdate | VolumeRegisterValue(volumePercent));

        sequence.clear();
        sequence.push_back({ dac1LeftVolumeRegister, volumeCode, 0 });
        sequence.push_back({ dac1RightVolumeRegister, volumeCode, 0 });
        sequence.push_back({ aif1Dac1Filters1Register, muted ? softMute : unmuteWithRamp, 0 });
        RunSequence();
    }

    void Wm8994::BeginPowerDown()
    {
        sequence.clear();
        Append(infra::MakeRange(shutDownSteps));
        RunSequence();
    }

    void Wm8994::RunSequence()
    {
        stepIndex = 0;
        NextStep();
    }

    void Wm8994::NextStep()
    {
        if (stepIndex == sequence.size())
            return SequenceDone();

        const Step& step = sequence[stepIndex];
        valueBytes = { static_cast<uint8_t>(step.value >> 8), static_cast<uint8_t>(step.value) };
        bus.WriteRegister(step.address, infra::MakeByteRange(valueBytes), [this]()
            {
                StepWritten();
            });
    }

    void Wm8994::StepWritten()
    {
        const uint16_t delay = sequence[stepIndex].delayAfterInMilliseconds;
        ++stepIndex;

        if (delay == 0)
            NextStep();
        else
            timer.Start(std::chrono::milliseconds(delay), [this]()
                {
                    NextStep();
                });
    }
}
