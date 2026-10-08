#ifndef DRIVERS_AUDIO_CODEC_CODEC_AUDIO_OUTPUT_HPP
#define DRIVERS_AUDIO_CODEC_CODEC_AUDIO_OUTPUT_HPP

#include "hal/interfaces/AudioOutput.hpp"
#include "infra/util/AutoResetFunction.hpp"
#include "infra/util/Function.hpp"
#include <cstdint>

namespace drivers
{
    class CodecAudioOutput
        : public hal::AudioOutput
    {
    public:
        static constexpr uint8_t maxVolumePercent = 100;

        CodecAudioOutput(const CodecAudioOutput& other) = delete;
        CodecAudioOutput& operator=(const CodecAudioOutput& other) = delete;

        void Start(hal::AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun) override;
        void Stop(const infra::Function<void()>& onStopped) override;
        void SetVolume(uint8_t percent, const infra::Function<void()>& onDone) override;
        void SetMuted(bool muted, const infra::Function<void()>& onDone) override;

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

        enum class RequestState : uint8_t
        {
            none,
            pending,
            applying,
            reporting
        };

        struct LevelRequest
        {
            RequestState state{ RequestState::none };
            infra::AutoResetFunction<void()> done;
        };

        void BeginStartUp(hal::AudioFormat format);
        void ApplyState();
        void BeginShutDown();
        void FinishShutDown();
        void StreamStopped();
        void ReportStopped();

        void Submit(LevelRequest& request, const infra::Function<void()>& onDone);
        bool LevelCanBeApplied() const;
        void ReportRequests(RequestState state);
        static void StartApplying(LevelRequest& request);
        static void Report(LevelRequest& request, RequestState state);

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
        infra::Function<void(Samples)> samplesCallback;
        infra::Function<void()> underrunCallback;
        infra::AutoResetFunction<void()> stopped;
        LevelRequest volume;
        LevelRequest mute;
    };
}

#endif
