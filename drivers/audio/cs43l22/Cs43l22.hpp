#ifndef DRIVERS_AUDIO_CS43L22_CS43L22_HPP
#define DRIVERS_AUDIO_CS43L22_CS43L22_HPP

#include "drivers/audio/codec/CodecAudioOutput.hpp"
#include "hal/interfaces/Gpio.hpp"
#include "infra/util/SharedPtr.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "services/util/RegisterStepRunner.hpp"
#include <cstdint>
#include <optional>

namespace drivers
{
    class Cs43l22
        : public CodecAudioOutput
    {
    public:
        enum class Output : uint8_t
        {
            headphone,
            speaker,
            both,
            automatic
        };

        enum class AnalogInput : uint8_t
        {
            ain1,
            ain2,
            ain3,
            ain4
        };

        struct Config
        {
            Output output;
            uint8_t initialVolume;
            uint16_t masterClockRatio;
            std::optional<AnalogInput> passthrough;
        };

        Cs43l22(services::RegisterBusAccess& bus, hal::AudioOutput& stream, hal::GpioPin& resetPin, const Config& config);

        static bool IsSupported(hal::AudioFormat format, uint16_t masterClockRatio, Output output);
        static uint8_t VolumeRegisterValue(uint8_t percent);
        static uint8_t PassthroughVolumeRegisterValue(uint8_t percent);

    private:
        bool Supports(hal::AudioFormat format) const override;
        void BeginBringUp(hal::AudioFormat format) override;
        void BeginApplyLevel(uint8_t volumePercent, bool muted) override;
        void BeginPowerDown() override;

        void PushReset();
        void PushIdentification();
        void PushConfiguration(hal::AudioFormat format);
        void PushPassthroughRouting(AnalogInput input);
        void PushRequiredInitialization();
        void PushPowerUp();
        void PushMute();
        void PushPassthroughLevel(uint8_t volumePercent, bool muted);
        void VerifyChipId() const;
        void InitializeAndPowerUp();
        void StartSequence();
        uint8_t OutputsPowerValue() const;

    private:
        hal::OutputPin resetPin;
        Config config;
        infra::AccessedBySharedPtr sharedAccess{ infra::emptyFunction };
        services::RegisterStepRunner runner;
        uint8_t chipId{ 0 };
    };
}

#endif
