#ifndef DRIVERS_AUDIO_WM8994_WM8994_HPP
#define DRIVERS_AUDIO_WM8994_WM8994_HPP

#include "drivers/audio/codec/CodecAudioOutput.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace drivers
{
    class Wm8994
        : public CodecAudioOutput
    {
    public:
        enum class Output : uint8_t
        {
            headphone,
            speaker
        };

        struct Config
        {
            Output output;
            uint8_t initialVolume;
        };

        struct Step
        {
            uint16_t address;
            uint16_t value;
            uint16_t delayAfterInMilliseconds;
        };

        Wm8994(services::RegisterBusAccessHalfWord& bus, hal::AudioOutput& stream, const Config& config);
        Wm8994(const Wm8994& other) = delete;
        Wm8994& operator=(const Wm8994& other) = delete;
        ~Wm8994();

        static bool IsSupported(hal::AudioFormat format);
        static uint8_t VolumeRegisterValue(uint8_t percent);

    private:
        static constexpr std::size_t maxSteps = 32;

        bool Supports(hal::AudioFormat format) const override;
        void BeginBringUp(hal::AudioFormat format) override;
        void BeginApplyLevel(uint8_t volumePercent, bool muted) override;
        void BeginPowerDown() override;

        void ChipIdRead();
        void BuildStartUp();
        void Append(infra::MemoryRange<const Step> steps);

        void RunSequence();
        void NextStep();
        void StepWritten();

    private:
        services::RegisterBusAccessHalfWord& bus;
        Config config;
        infra::TimerSingleShot timer;

        uint16_t rateRegisterValue{ 0 };
        infra::BoundedVector<Step>::WithMaxSize<maxSteps> sequence;
        std::size_t stepIndex{ 0 };
        std::array<uint8_t, 2> valueBytes{};
        std::array<uint8_t, 2> chipIdBytes{};
    };
}

#endif
