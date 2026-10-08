#include "drivers/audio/codec/CodecAudioOutput.hpp"
#include "infra/event/EventDispatcher.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    CodecAudioOutput::CodecAudioOutput(hal::AudioOutput& stream, uint8_t initialVolume)
        : stream(stream)
        , volumePercent(initialVolume)
    {
        really_assert(initialVolume <= maxVolumePercent);
    }

    CodecAudioOutput::~CodecAudioOutput()
    {
        really_assert(!stopped);

        if (phase != Phase::idle)
            stream.Stop();
    }

    void CodecAudioOutput::Start(hal::AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun)
    {
        really_assert(Supports(format));
        really_assert(!requested);
        really_assert(!stopped);

        requested = format;
        samplesCallback = onSamplesRequired;
        underrunCallback = onUnderrun;

        if (phase == Phase::idle)
            BeginStartUp();
    }

    void CodecAudioOutput::Stop()
    {
        if (!requested)
            return;

        requested.reset();
        samplesCallback = nullptr;
        underrunCallback = nullptr;

        ReconcileIfPossible();
    }

    void CodecAudioOutput::Stop(const infra::Function<void()>& onStopped)
    {
        really_assert(!stopped);

        stopped = onStopped;

        Stop();
        ReportStoppedWhenIdle();
    }

    void CodecAudioOutput::SetVolume(uint8_t percent)
    {
        really_assert(percent <= maxVolumePercent);

        volumePercent = percent;
        stateDirty = true;

        ReconcileIfPossible();
    }

    void CodecAudioOutput::SetMuted(bool muted)
    {
        outputMuted = muted;
        stateDirty = true;

        ReconcileIfPossible();
    }

    void CodecAudioOutput::SequenceDone()
    {
        busy = false;
        Reconcile();
    }

    bool CodecAudioOutput::Busy() const
    {
        return busy;
    }

    void CodecAudioOutput::BeginStartUp()
    {
        phase = Phase::startingUp;
        busy = true;
        activeFormat = requested;
        stateDirty = true;

        stream.Start(
            *activeFormat, [this](Samples toFill)
            {
                SamplesRequired(toFill);
            },
            [this]()
            {
                Underrun();
            });

        BeginBringUp(*activeFormat);
    }

    void CodecAudioOutput::ApplyState()
    {
        stateDirty = false;
        busy = true;
        BeginApplyLevel(volumePercent, outputMuted);
    }

    void CodecAudioOutput::BeginShutDown()
    {
        phase = Phase::shuttingDown;
        busy = true;
        BeginPowerDown();
    }

    void CodecAudioOutput::FinishShutDown()
    {
        phase = Phase::idle;
        activeFormat.reset();
        stream.Stop();

        if (requested)
            BeginStartUp();

        ReportStoppedWhenIdle();
    }

    void CodecAudioOutput::ReportStoppedWhenIdle()
    {
        if (phase == Phase::idle && stopped)
            infra::EventDispatcher::Instance().Schedule([this]()
                {
                    stopped();
                });
    }

    void CodecAudioOutput::Reconcile()
    {
        if (phase == Phase::startingUp)
            ReconcileStartingUp();
        else if (phase == Phase::playing)
            ReconcilePlaying();
        else if (phase == Phase::shuttingDown)
            FinishShutDown();
    }

    void CodecAudioOutput::ReconcileStartingUp()
    {
        if (requested != activeFormat)
            BeginShutDown();
        else if (stateDirty)
            ApplyState();
        else
            phase = Phase::playing;
    }

    void CodecAudioOutput::ReconcilePlaying()
    {
        if (requested != activeFormat)
            BeginShutDown();
        else if (stateDirty)
            ApplyState();
    }

    void CodecAudioOutput::ReconcileIfPossible()
    {
        if (phase == Phase::playing && !busy)
            Reconcile();
    }

    void CodecAudioOutput::SamplesRequired(Samples toFill)
    {
        if (phase == Phase::playing && samplesCallback)
        {
            auto callback = samplesCallback;
            callback(toFill);
        }
        else
            std::fill(toFill.begin(), toFill.end(), int16_t{ 0 });
    }

    void CodecAudioOutput::Underrun()
    {
        if (phase == Phase::playing && underrunCallback)
        {
            auto callback = underrunCallback;
            callback();
        }
    }
}
