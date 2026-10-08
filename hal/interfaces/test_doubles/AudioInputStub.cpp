#include "hal/interfaces/test_doubles/AudioInputStub.hpp"

namespace hal
{
    AudioInputStub::AudioInputStub()
    {
        ON_CALL(*this, Start).WillByDefault([this](AudioFormat, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun)
            {
                this->onSamples = onSamples;
                this->onOverrun = onOverrun;
            });

        ON_CALL(*this, Stop).WillByDefault([this](const infra::Function<void()>& onStopped)
            {
                onSamples = nullptr;
                onOverrun = nullptr;
                onStopped();
            });
    }

    void AudioInputStub::PeriodCaptured(Samples samples)
    {
        if (onSamples)
        {
            auto callback = onSamples;
            callback(samples);
        }
    }

    void AudioInputStub::Overrun()
    {
        if (onOverrun)
        {
            auto callback = onOverrun;
            callback();
        }
    }
}
