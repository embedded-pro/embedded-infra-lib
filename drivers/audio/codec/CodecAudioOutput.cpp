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
        really_assert(volume.state == RequestState::none);
        really_assert(mute.state == RequestState::none);

        if (phase != Phase::idle)
            stream.Stop(infra::emptyFunction);
    }

    void CodecAudioOutput::Start(hal::AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun)
    {
        really_assert(Supports(format));
        really_assert(phase == Phase::idle);
        really_assert(!stopped);

        samplesCallback = onSamplesRequired;
        underrunCallback = onUnderrun;

        BeginStartUp(format);
    }

    void CodecAudioOutput::Stop(const infra::Function<void()>& onStopped)
    {
        really_assert(onStopped != nullptr);
        really_assert(!stopped);

        stopped = onStopped;
        samplesCallback = nullptr;
        underrunCallback = nullptr;
        ReportRequests(RequestState::pending);

        if (phase == Phase::idle)
            ReportStopped();
        else
            ReconcileIfPossible();
    }

    void CodecAudioOutput::SetVolume(uint8_t percent, const infra::Function<void()>& onDone)
    {
        really_assert(percent <= maxVolumePercent);

        volumePercent = percent;
        Submit(volume, onDone);
    }

    void CodecAudioOutput::SetMuted(bool muted, const infra::Function<void()>& onDone)
    {
        outputMuted = muted;
        Submit(mute, onDone);
    }

    void CodecAudioOutput::SequenceDone()
    {
        busy = false;
        ReportRequests(RequestState::applying);
        Reconcile();
    }

    bool CodecAudioOutput::Busy() const
    {
        return busy;
    }

    void CodecAudioOutput::BeginStartUp(hal::AudioFormat format)
    {
        phase = Phase::startingUp;
        busy = true;
        stateDirty = true;

        stream.Start(
            format, [this](Samples toFill)
            {
                SamplesRequired(toFill);
            },
            [this]()
            {
                Underrun();
            });

        BeginBringUp(format);
    }

    void CodecAudioOutput::ApplyState()
    {
        stateDirty = false;
        busy = true;
        StartApplying(volume);
        StartApplying(mute);
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
        stream.Stop([this]()
            {
                StreamStopped();
            });
    }

    void CodecAudioOutput::StreamStopped()
    {
        phase = Phase::idle;
        ReportStopped();
    }

    void CodecAudioOutput::ReportStopped()
    {
        infra::EventDispatcher::Instance().Schedule([this]()
            {
                stopped();
            });
    }

    void CodecAudioOutput::Submit(LevelRequest& request, const infra::Function<void()>& onDone)
    {
        really_assert(onDone != nullptr);
        really_assert(request.state == RequestState::none);

        request.done = onDone;
        request.state = RequestState::pending;

        if (LevelCanBeApplied())
        {
            stateDirty = true;
            ReconcileIfPossible();
        }
        else
            ReportRequests(RequestState::pending);
    }

    bool CodecAudioOutput::LevelCanBeApplied() const
    {
        return (phase == Phase::startingUp || phase == Phase::playing) && !stopped;
    }

    void CodecAudioOutput::ReportRequests(RequestState state)
    {
        Report(volume, state);
        Report(mute, state);
    }

    void CodecAudioOutput::StartApplying(LevelRequest& request)
    {
        if (request.state == RequestState::pending)
            request.state = RequestState::applying;
    }

    void CodecAudioOutput::Report(LevelRequest& request, RequestState state)
    {
        if (request.state != state)
            return;

        request.state = RequestState::reporting;
        infra::EventDispatcher::Instance().Schedule([&request]()
            {
                request.state = RequestState::none;
                request.done();
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
        if (stopped)
            BeginShutDown();
        else if (stateDirty)
            ApplyState();
        else
            phase = Phase::playing;
    }

    void CodecAudioOutput::ReconcilePlaying()
    {
        if (stopped)
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
