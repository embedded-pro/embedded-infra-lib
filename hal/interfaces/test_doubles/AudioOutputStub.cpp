#include "hal/interfaces/test_doubles/AudioOutputStub.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace hal
{
    AudioOutputStub::AudioOutputStub(infra::MemoryRange<int16_t> storage)
        : storage(storage)
    {
        ON_CALL(*this, Start).WillByDefault([this](AudioFormat, const infra::Function<void(Samples)>& onSamplesRequired, const infra::Function<void()>& onUnderrun)
            {
                this->onSamplesRequired = onSamplesRequired;
                this->onUnderrun = onUnderrun;
            });

        ON_CALL(*this, Stop).WillByDefault([this]()
            {
                onSamplesRequired = nullptr;
                onUnderrun = nullptr;
            });

        ON_CALL(*this, SetVolume).WillByDefault([this](uint8_t percent)
            {
                really_assert(percent <= 100);
                volume = percent;
            });

        ON_CALL(*this, SetMuted).WillByDefault([this](bool value)
            {
                muted = value;
            });
    }

    void AudioOutputStub::PeriodElapsed(std::size_t numberOfSamples)
    {
        really_assert(numberOfSamples <= storage.size());

        if (onSamplesRequired)
        {
            auto period = infra::Head(storage, numberOfSamples);
            std::fill(period.begin(), period.end(), 0);
            lastPeriodSize = numberOfSamples;

            auto callback = onSamplesRequired;
            callback(period);
        }
    }

    void AudioOutputStub::Underrun()
    {
        if (onUnderrun)
        {
            auto callback = onUnderrun;
            callback();
        }
    }

    infra::MemoryRange<const int16_t> AudioOutputStub::LastPeriod() const
    {
        return infra::Head(infra::MemoryRange<const int16_t>{ storage }, lastPeriodSize);
    }

    uint8_t AudioOutputStub::Volume() const
    {
        return volume;
    }

    bool AudioOutputStub::Muted() const
    {
        return muted;
    }
}
