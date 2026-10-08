#ifndef DRIVERS_AUDIO_CODEC_CODEC_AUDIO_OUTPUT_HPP
#define DRIVERS_AUDIO_CODEC_CODEC_AUDIO_OUTPUT_HPP

#include "hal/interfaces/AudioOutput.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include "services/util/Stoppable.hpp"
#include <cstdint>
#include <optional>

namespace drivers
{
    class CodecAudioOutput
        : public hal::AudioOutput
        , public services::Stoppable
    {
    public:
        static constexpr uint8_t maxVolumePercent = 100;

        CodecAudioOutput(const CodecAudioOutput& other) = delete;
        CodecAudioOutput& operator=(const CodecAudioOutput& other) = delete;

        void Start(hal::AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun) override;
        void Stop() override;
        void Stop(const infra::Function<void()>& onStopped) override;
        void SetVolume(uint8_t percent) override;
        void SetMuted(bool muted) override;

    protected:
        CodecAudioOutput(hal::AudioOutput& stream, uint8_t initialVolume);
        ~CodecAudioOutput();

        virtual bool Supports(hal::AudioFormat format) const = 0;
        virtual void BeginBringUp(hal::AudioFormat format) = 0;
        virtual void BeginApplyLevel(uint8_t volumePercent, bool muted) = 0;
        virtual void BeginPowerDown() = 0;

        void SequenceDone();
        bool Busy() const;

    private:
        enum class Phase : uint8_t
        {
            idle,
            startingUp,
            playing,
            shuttingDown
        };

        void BeginStartUp();
        void ApplyState();
        void BeginShutDown();
        void FinishShutDown();
        void ReportStoppedWhenIdle();

        void Reconcile();
        void ReconcileStartingUp();
        void ReconcilePlaying();
        void ReconcileIfPossible();

        void SamplesRequired(Samples toFill);
        void Underrun();

    private:
        hal::AudioOutput& stream;
        uint8_t volumePercent;
        bool outputMuted{ false };

        Phase phase{ Phase::idle };
        bool busy{ false };
        bool stateDirty{ false };
        std::optional<hal::AudioFormat> requested;
        std::optional<hal::AudioFormat> activeFormat;
        infra::Function<void(Samples)> samplesCallback;
        infra::Function<void()> underrunCallback;
        infra::AutoResetFunction<void()> stopped;
    };
}

#endif
