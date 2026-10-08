#include "drivers/audio/cs43l22/Cs43l22.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>

namespace drivers
{
    namespace
    {
        using Runner = services::RegisterStepRunner;

        struct VolumeRange
        {
            int minCode;
            int steps;
        };

        constexpr uint8_t initializationKeyRegister = 0x00;
        constexpr uint8_t chipIdRegister = 0x01;
        constexpr uint8_t powerControl1Register = 0x02;
        constexpr uint8_t powerControl2Register = 0x04;
        constexpr uint8_t clockingControlRegister = 0x05;
        constexpr uint8_t interfaceControl1Register = 0x06;
        constexpr uint8_t passthroughSelectARegister = 0x08;
        constexpr uint8_t passthroughSelectBRegister = 0x09;
        constexpr uint8_t analogRampAndZeroCrossRegister = 0x0a;
        constexpr uint8_t playbackControl1Register = 0x0d;
        constexpr uint8_t miscellaneousControlsRegister = 0x0e;
        constexpr uint8_t passthroughVolumeARegister = 0x14;
        constexpr uint8_t passthroughVolumeBRegister = 0x15;
        constexpr uint8_t masterVolumeARegister = 0x20;
        constexpr uint8_t masterVolumeBRegister = 0x21;
        constexpr uint8_t initializationRegister32 = 0x32;
        constexpr uint8_t initializationRegister47 = 0x47;

        constexpr uint8_t expectedChipId = 0xe0;
        constexpr uint8_t chipIdMask = 0xf8;

        constexpr uint8_t poweredDown = 0x9f;
        constexpr uint8_t poweredUp = 0x9e;

        constexpr uint8_t initializationUnlock = 0x99;
        constexpr uint8_t initializationLock = 0x00;
        constexpr uint8_t initializationValue47 = 0x80;
        constexpr uint8_t initializationBit32 = 0x80;

        constexpr std::array<uint8_t, 4> outputsOn{ 0xaf, 0xfa, 0xaa, 0x05 };

        constexpr uint8_t autoDetectClocking = 0xa0;
        constexpr uint8_t masterClockDivideBy2 = 0x01;
        constexpr uint8_t slaveI2s = 0x04;

        constexpr uint8_t masterMute = 0x03;
        constexpr uint8_t passthroughSelectMask = 0x0f;
        constexpr uint8_t passthroughEnable = 0xc0;
        constexpr uint8_t passthroughMute = 0x30;
        constexpr uint8_t digitalSoftRampAndZeroCross = 0x03;
        constexpr uint8_t analogSoftRampAndZeroCross = 0x0f;

        constexpr VolumeRange masterVolume{ 0x34, 204 };
        constexpr VolumeRange passthroughVolume{ 0x88, 120 };

        constexpr uint32_t minSampleRate = 4000;
        constexpr uint32_t maxSampleRate = 96000;
        constexpr uint8_t supportedChannels = 2;

        constexpr uint32_t quarterSpeedMaxSampleRate = 12500;
        constexpr uint32_t halfSpeedMaxSampleRate = 25000;
        constexpr uint32_t singleSpeedMaxSampleRate = 50000;
        constexpr std::array<uint64_t, 2> pwmForbiddenMasterClocks{ 16934400, 18432000 };

        constexpr std::chrono::milliseconds resetPulse{ 5 };
        constexpr std::chrono::milliseconds resetSettle{ 5 };
        constexpr std::chrono::milliseconds muteRamp{ 100 };
        constexpr std::chrono::milliseconds powerDownSettle{ 1 };

        uint8_t VolumeCode(const VolumeRange& range, uint8_t percent)
        {
            really_assert(percent <= CodecAudioOutput::maxVolumePercent);

            const int steps = (percent * range.steps + CodecAudioOutput::maxVolumePercent / 2) / CodecAudioOutput::maxVolumePercent;

            return static_cast<uint8_t>(range.minCode + steps);
        }

        uint8_t BitsIf(bool condition, uint8_t bits)
        {
            return condition ? bits : uint8_t{ 0 };
        }

        uint16_t BaseRatio(uint32_t sampleRate)
        {
            if (sampleRate <= quarterSpeedMaxSampleRate)
                return 1024;

            if (sampleRate <= halfSpeedMaxSampleRate)
                return 512;

            if (sampleRate <= singleSpeedMaxSampleRate)
                return 256;

            return 128;
        }

        std::optional<bool> DivideBy2(uint32_t sampleRate, uint16_t masterClockRatio)
        {
            const uint16_t base = BaseRatio(sampleRate);

            if (masterClockRatio == base || masterClockRatio == base * 3 / 2)
                return false;

            if (masterClockRatio == base * 2 || masterClockRatio == base * 3)
                return true;

            return std::nullopt;
        }

        bool IsPwmForbiddenMasterClock(uint32_t sampleRate, uint16_t masterClockRatio)
        {
            const uint64_t masterClock = uint64_t{ sampleRate } * masterClockRatio;

            return masterClock == pwmForbiddenMasterClocks[0] || masterClock == pwmForbiddenMasterClocks[1];
        }

        bool MayDriveSpeaker(Cs43l22::Output output)
        {
            return output != Cs43l22::Output::headphone;
        }
    }

    Cs43l22::Cs43l22(services::RegisterBusAccess& bus, hal::AudioOutput& stream, hal::GpioPin& resetPin, const Config& config)
        : CodecAudioOutput(stream, config.initialVolume)
        , resetPin(resetPin)
        , config(config)
        , runner(bus, sharedAccess)
    {}

    bool Cs43l22::IsSupported(hal::AudioFormat format, uint16_t masterClockRatio, Output output)
    {
        return format.channels == supportedChannels && format.sampleRate >= minSampleRate && format.sampleRate <= maxSampleRate && DivideBy2(format.sampleRate, masterClockRatio).has_value() && !(MayDriveSpeaker(output) && IsPwmForbiddenMasterClock(format.sampleRate, masterClockRatio));
    }

    uint8_t Cs43l22::VolumeRegisterValue(uint8_t percent)
    {
        return VolumeCode(masterVolume, percent);
    }

    uint8_t Cs43l22::PassthroughVolumeRegisterValue(uint8_t percent)
    {
        return VolumeCode(passthroughVolume, percent);
    }

    bool Cs43l22::Supports(hal::AudioFormat format) const
    {
        return IsSupported(format, config.masterClockRatio, config.output);
    }

    void Cs43l22::BeginBringUp(hal::AudioFormat format)
    {
        runner.Clear();
        PushReset();
        PushIdentification();
        PushConfiguration(format);

        if (config.passthrough)
            PushPassthroughRouting(*config.passthrough);

        runner.Start([this]()
            {
                InitializeAndPowerUp();
            });
    }

    void Cs43l22::InitializeAndPowerUp()
    {
        runner.Clear();
        PushRequiredInitialization();
        PushPowerUp();
        StartSequence();
    }

    void Cs43l22::BeginApplyLevel(uint8_t volumePercent, bool muted)
    {
        const uint8_t volumeCode = VolumeRegisterValue(volumePercent);

        runner.Clear();
        runner.Push(Runner::WriteRegister{ masterVolumeARegister, volumeCode });
        runner.Push(Runner::WriteRegister{ masterVolumeBRegister, volumeCode });
        runner.Push(Runner::ModifyRegister{ playbackControl1Register, masterMute, BitsIf(muted, masterMute) });

        if (config.passthrough)
            PushPassthroughLevel(volumePercent, muted);

        StartSequence();
    }

    void Cs43l22::BeginPowerDown()
    {
        runner.Clear();
        PushMute();
        runner.Push(Runner::Delay{ muteRamp });
        runner.Push(Runner::ModifyRegister{ miscellaneousControlsRegister, digitalSoftRampAndZeroCross, 0x00 });
        runner.Push(Runner::ModifyRegister{ analogRampAndZeroCrossRegister, analogSoftRampAndZeroCross, 0x00 });
        runner.Push(Runner::WriteRegister{ powerControl1Register, poweredDown });
        runner.Push(Runner::Delay{ powerDownSettle });
        runner.Push(Runner::Invoke{ [this]()
            {
                resetPin.Set(false);
            } });
        StartSequence();
    }

    void Cs43l22::PushReset()
    {
        runner.Push(Runner::Invoke{ [this]()
            {
                resetPin.Set(false);
            } });
        runner.Push(Runner::Delay{ resetPulse });
        runner.Push(Runner::Invoke{ [this]()
            {
                resetPin.Set(true);
            } });
        runner.Push(Runner::Delay{ resetSettle });
    }

    void Cs43l22::PushIdentification()
    {
        runner.Push(Runner::ReadBurst{ chipIdRegister, infra::MakeByteRange(chipId) });
        runner.Push(Runner::Invoke{ [this]()
            {
                VerifyChipId();
            } });
    }

    void Cs43l22::PushConfiguration(hal::AudioFormat format)
    {
        const uint8_t clocking = static_cast<uint8_t>(autoDetectClocking | BitsIf(*DivideBy2(format.sampleRate, config.masterClockRatio), masterClockDivideBy2));

        runner.Push(Runner::WriteRegister{ powerControl2Register, OutputsPowerValue() });
        runner.Push(Runner::WriteRegister{ clockingControlRegister, clocking });
        runner.Push(Runner::WriteRegister{ interfaceControl1Register, slaveI2s });
        runner.Push(Runner::ModifyRegister{ playbackControl1Register, 0x00, masterMute });
    }

    void Cs43l22::PushPassthroughRouting(AnalogInput input)
    {
        const uint8_t select = static_cast<uint8_t>(1 << static_cast<uint8_t>(input));

        runner.Push(Runner::ModifyRegister{ passthroughSelectARegister, passthroughSelectMask, select });
        runner.Push(Runner::ModifyRegister{ passthroughSelectBRegister, passthroughSelectMask, select });
        runner.Push(Runner::ModifyRegister{ miscellaneousControlsRegister, 0x00, passthroughEnable | passthroughMute });
    }

    void Cs43l22::PushRequiredInitialization()
    {
        runner.Push(Runner::WriteRegister{ initializationKeyRegister, initializationUnlock });
        runner.Push(Runner::WriteRegister{ initializationRegister47, initializationValue47 });
        runner.Push(Runner::ModifyRegister{ initializationRegister32, 0x00, initializationBit32 });
        runner.Push(Runner::ModifyRegister{ initializationRegister32, initializationBit32, 0x00 });
        runner.Push(Runner::WriteRegister{ initializationKeyRegister, initializationLock });
    }

    void Cs43l22::PushPowerUp()
    {
        runner.Push(Runner::WriteRegister{ powerControl1Register, poweredUp });
    }

    void Cs43l22::PushMute()
    {
        runner.Push(Runner::ModifyRegister{ playbackControl1Register, 0x00, masterMute });

        if (config.passthrough)
            runner.Push(Runner::ModifyRegister{ miscellaneousControlsRegister, 0x00, passthroughMute });
    }

    void Cs43l22::PushPassthroughLevel(uint8_t volumePercent, bool muted)
    {
        const uint8_t volumeCode = PassthroughVolumeRegisterValue(volumePercent);

        runner.Push(Runner::WriteRegister{ passthroughVolumeARegister, volumeCode });
        runner.Push(Runner::WriteRegister{ passthroughVolumeBRegister, volumeCode });
        runner.Push(Runner::ModifyRegister{ miscellaneousControlsRegister, passthroughMute, BitsIf(muted, passthroughMute) });
    }

    void Cs43l22::VerifyChipId() const
    {
        really_assert((chipId & chipIdMask) == expectedChipId);
    }

    void Cs43l22::StartSequence()
    {
        runner.Start([this]()
            {
                SequenceDone();
            });
    }

    uint8_t Cs43l22::OutputsPowerValue() const
    {
        return outputsOn[static_cast<std::size_t>(config.output)];
    }
}
