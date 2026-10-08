#include "drivers/microphones/pdm/PdmMicrophone.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    namespace
    {
        constexpr std::size_t millisecondsPerSecond{ 1000 };
        constexpr uint32_t bitsPerWord{ 16 };
    }

    PdmMicrophone::PdmMicrophone(const Limits& limits, hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage)
        : limits{ limits }
        , bitStream{ bitStream }
        , converter{ converter }
        , periodStorage{ periodStorage }
    {}

    PdmMicrophone::~PdmMicrophone()
    {
        Stop();
    }

    void PdmMicrophone::Start(hal::AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun)
    {
        really_assert(!running);
        really_assert(format.channels == 1 || format.channels == 2);

        const uint32_t clockFrequency = ClockFrequency(format);
        const std::size_t startupFrames = static_cast<std::size_t>(format.sampleRate) * limits.startupTimeInMilliseconds / millisecondsPerSecond;

        this->onSamples = onSamples;
        this->onOverrun = onOverrun;
        startupSamplesToDiscard = startupFrames * format.channels;
        running = true;

        converter.Reset(format.channels, format.sampleRate);
        bitStream.Start(
            { clockFrequency / bitsPerWord, format.channels }, [this](Samples words)
            {
                WordsCaptured(words);
            },
            [this]()
            {
                Overrun();
            });
    }

    void PdmMicrophone::Stop()
    {
        if (running)
        {
            running = false;
            bitStream.Stop();
            onSamples = nullptr;
            onOverrun = nullptr;
        }
    }

    uint32_t PdmMicrophone::ClockFrequency(hal::AudioFormat format) const
    {
        const uint64_t clockFrequency = static_cast<uint64_t>(format.sampleRate) * converter.Decimation();
        really_assert(clockFrequency >= limits.minClockFrequency && clockFrequency <= limits.maxClockFrequency);
        really_assert(clockFrequency % bitsPerWord == 0);

        return static_cast<uint32_t>(clockFrequency);
    }

    void PdmMicrophone::WordsCaptured(Samples words)
    {
        really_assert(converter.MaxSamples(words.size()) <= periodStorage.size());

        const auto samples = DiscardStartup(infra::Head(periodStorage, converter.Convert(words, periodStorage)));

        if (!samples.empty())
        {
            auto callback = onSamples;
            callback(samples);
        }
    }

    infra::MemoryRange<int16_t> PdmMicrophone::DiscardStartup(infra::MemoryRange<int16_t> samples)
    {
        const std::size_t discarded = std::min(startupSamplesToDiscard, samples.size());
        startupSamplesToDiscard -= discarded;

        return infra::DiscardHead(samples, discarded);
    }

    void PdmMicrophone::Overrun()
    {
        auto callback = onOverrun;
        callback();
    }
}
