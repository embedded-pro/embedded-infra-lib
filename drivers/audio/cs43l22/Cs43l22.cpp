#include "drivers/audio/cs43l22/Cs43l22.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <array>
#include <chrono>

namespace drivers
{
    namespace
    {
        using Runner = services::RegisterStepRunner;

        constexpr uint8_t initializationKeyRegister = 0x00;
        constexpr uint8_t chipIdRegister = 0x01;
        constexpr uint8_t powerControl1Register = 0x02;
        constexpr uint8_t powerControl2Register = 0x04;
        constexpr uint8_t clockingControlRegister = 0x05;
        constexpr uint8_t interfaceControl1Register = 0x06;
        constexpr uint8_t passthroughSelectARegister = 0x08;
        constexpr uint8_t passthroughSelectBRegister = 0x09;
        constexpr uint8_t miscellaneousControlsRegister = 0x0e;
        constexpr uint8_t playbackControl2Register = 0x0f;
        constexpr uint8_t masterVolumeARegister = 0x20;
        constexpr uint8_t masterVolumeBRegister = 0x21;
        constexpr uint8_t initializationRegister32 = 0x32;
        constexpr uint8_t initializationRegister47 = 0x47;

        constexpr uint8_t expectedChipId = 0xe0;
        constexpr uint8_t chipIdMask = 0xf8;

        constexpr uint8_t poweredDown = 0x01;
        constexpr uint8_t poweredUp = 0x9e;

        constexpr uint8_t initializationUnlock = 0x99;
        constexpr uint8_t initializationLock = 0x00;
        constexpr uint8_t initializationValue47 = 0x80;
        constexpr uint8_t initializationBit32 = 0x80;

        constexpr uint8_t outputsOff = 0xff;
        constexpr std::array<uint8_t, 4> outputsOn{ 0xaf, 0xfa, 0xaa, 0x05 };

        constexpr uint8_t autoDetectClocking = 0x81;
        constexpr uint8_t slaveI2s = 0x04;
        constexpr uint8_t outputsUnmuted = 0x00;
        constexpr uint8_t passthroughEnable = 0xc0;

        constexpr int minVolumeCode = 0x34;
        constexpr int volumeSteps = 204;

        constexpr uint32_t minSampleRate = 4000;
        constexpr uint32_t maxSampleRate = 100000;
        constexpr uint8_t supportedChannels = 2;

        constexpr std::chrono::milliseconds resetPulse{ 5 };
        constexpr std::chrono::milliseconds resetSettle{ 5 };
        constexpr std::chrono::milliseconds powerDownSettle{ 100 };
    }

    Cs43l22::Cs43l22(services::RegisterBusAccess& bus, hal::AudioOutput& stream, hal::GpioPin& resetPin, const Config& config)
        : CodecAudioOutput(stream, config.initialVolume)
        , resetPin(resetPin)
        , config(config)
        , runner(bus, sharedAccess)
    {}

    bool Cs43l22::IsSupported(hal::AudioFormat format)
    {
        return format.channels == supportedChannels && format.sampleRate >= minSampleRate && format.sampleRate <= maxSampleRate;
    }

    uint8_t Cs43l22::VolumeRegisterValue(uint8_t percent)
    {
        really_assert(percent <= maxVolumePercent);

        const int steps = (percent * volumeSteps + maxVolumePercent / 2) / maxVolumePercent;

        return static_cast<uint8_t>(minVolumeCode + steps);
    }

    bool Cs43l22::Supports(hal::AudioFormat format) const
    {
        return IsSupported(format);
    }

    void Cs43l22::BeginBringUp(hal::AudioFormat)
    {
        runner.Clear();
        PushReset();
        PushIdentification();
        PushRequiredInitialization();
        runner.Start([this]()
            {
                ConfigureAndPowerUp();
            });
    }

    void Cs43l22::ConfigureAndPowerUp()
    {
        runner.Clear();
        PushConfiguration();

        if (config.passthrough)
            PushPassthrough(*config.passthrough);

        PushPowerUp();
        StartSequence();
    }

    void Cs43l22::BeginApplyLevel(uint8_t volumePercent, bool muted)
    {
        const uint8_t volumeCode = VolumeRegisterValue(volumePercent);

        runner.Clear();
        runner.Push(Runner::WriteRegister{ masterVolumeARegister, volumeCode });
        runner.Push(Runner::WriteRegister{ masterVolumeBRegister, volumeCode });
        runner.Push(Runner::WriteRegister{ powerControl2Register, muted ? outputsOff : OutputsPowerValue() });
        StartSequence();
    }

    void Cs43l22::BeginPowerDown()
    {
        runner.Clear();
        runner.Push(Runner::WriteRegister{ powerControl2Register, outputsOff });
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

    void Cs43l22::PushRequiredInitialization()
    {
        runner.Push(Runner::WriteRegister{ powerControl1Register, poweredDown });
        runner.Push(Runner::WriteRegister{ initializationKeyRegister, initializationUnlock });
        runner.Push(Runner::WriteRegister{ initializationRegister47, initializationValue47 });
        runner.Push(Runner::ModifyRegister{ initializationRegister32, 0x00, initializationBit32 });
        runner.Push(Runner::ModifyRegister{ initializationRegister32, initializationBit32, 0x00 });
        runner.Push(Runner::WriteRegister{ initializationKeyRegister, initializationLock });
    }

    void Cs43l22::PushConfiguration()
    {
        runner.Push(Runner::WriteRegister{ powerControl2Register, outputsOff });
        runner.Push(Runner::WriteRegister{ clockingControlRegister, autoDetectClocking });
        runner.Push(Runner::WriteRegister{ interfaceControl1Register, slaveI2s });
        runner.Push(Runner::WriteRegister{ playbackControl2Register, outputsUnmuted });
    }

    void Cs43l22::PushPassthrough(AnalogInput input)
    {
        const uint8_t select = static_cast<uint8_t>(1 << static_cast<uint8_t>(input));

        runner.Push(Runner::WriteRegister{ passthroughSelectARegister, select });
        runner.Push(Runner::WriteRegister{ passthroughSelectBRegister, select });
        runner.Push(Runner::ModifyRegister{ miscellaneousControlsRegister, 0x00, passthroughEnable });
    }

    void Cs43l22::PushPowerUp()
    {
        runner.Push(Runner::WriteRegister{ powerControl1Register, poweredUp });
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
