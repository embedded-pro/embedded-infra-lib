#ifndef DRIVERS_AUDIO_WM8994_WM8994_HPP
#define DRIVERS_AUDIO_WM8994_WM8994_HPP

#include "hal/interfaces/AudioOutput.hpp"
#include "infra/timer/Timer.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/BoundedVector.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include "services/util/RegisterBusAccess.hpp"
#include "services/util/Stoppable.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace drivers
{
    class Wm8994
        : public hal::AudioOutput
        , public services::Stoppable
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

        void Start(hal::AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun) override;
        void Stop() override;
        void Stop(const infra::Function<void()>& onStopped) override;
        void SetVolume(uint8_t percent) override;
        void SetMuted(bool muted) override;

    private:
        enum class Phase : uint8_t
        {
            idle,
            startingUp,
            playing,
            shuttingDown
        };

        static constexpr std::size_t maxSteps = 32;

        void BeginStartUp();
        void ChipIdRead();
        void BuildStartUp();
        void Append(infra::MemoryRange<const Step> steps);
        void ApplyState();
        void BeginShutDown();
        void FinishShutDown();

        void RunSequence();
        void NextStep();
        void StepWritten();
        void SequenceDone();

        void ReportStoppedWhenIdle();

        void Reconcile();
        void ReconcileStartingUp();
        void ReconcilePlaying();
        void ReconcileIfPossible();

        void SamplesRequired(Samples toFill);
        void Underrun();

    private:
        services::RegisterBusAccessHalfWord& bus;
        hal::AudioOutput& stream;
        Config config;
        uint8_t volumePercent;
        bool outputMuted{ false };
        infra::TimerSingleShot timer;

        Phase phase{ Phase::idle };
        bool busy{ false };
        bool stateDirty{ false };
        std::optional<hal::AudioFormat> requested;
        std::optional<hal::AudioFormat> activeFormat;
        infra::Function<void(Samples)> samplesCallback;
        infra::Function<void()> underrunCallback;
        infra::AutoResetFunction<void()> stopped;

        infra::BoundedVector<Step>::WithMaxSize<maxSteps> sequence;
        std::size_t stepIndex{ 0 };
        std::array<uint8_t, 2> valueBytes{};
        std::array<uint8_t, 2> chipIdBytes{};
    };
}

#endif
