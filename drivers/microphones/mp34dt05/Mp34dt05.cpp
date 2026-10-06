#include "drivers/microphones/mp34dt05/Mp34dt05.hpp"
#include "infra/util/ReallyAssert.hpp"
#include <algorithm>

namespace drivers
{
    namespace
    {
        constexpr std::size_t millisecondsPerSecond{ 1000 };
        constexpr std::size_t bitsPerByte{ 8 };
    }

    Mp34dt05::Mp34dt05(hal::PdmInput& input, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage)
        : input{ input }
        , converter{ converter }
        , periodStorage{ periodStorage }
    {}

    Mp34dt05::~Mp34dt05()
    {
        Stop();
    }

    void Mp34dt05::Start(hal::AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun)
    {
        really_assert(!running);
        really_assert(format.channels == 1 || format.channels == 2);

        const uint32_t clockFrequency = ClockFrequency(format);
        const std::size_t startupFrames = static_cast<std::size_t>(format.sampleRate) * startupTimeInMilliseconds / millisecondsPerSecond;

        this->onSamples = onSamples;
        this->onOverrun = onOverrun;
        startupSamplesToDiscard = startupFrames * format.channels;
        running = true;

        converter.Reset(format.channels, format.sampleRate);
        input.Start(
            { clockFrequency, format.channels }, [this](hal::PdmInput::Bits bits)
            {
                BitsCaptured(bits);
            },
            [this]()
            {
                Overrun();
            });
    }

    void Mp34dt05::Stop()
    {
        if (running)
        {
            running = false;
            input.Stop();
            onSamples = nullptr;
            onOverrun = nullptr;
        }
    }

    uint32_t Mp34dt05::ClockFrequency(hal::AudioFormat format) const
    {
        const uint64_t clockFrequency = static_cast<uint64_t>(format.sampleRate) * converter.Decimation();
        really_assert(clockFrequency >= minClockFrequency && clockFrequency <= maxClockFrequency);

        return static_cast<uint32_t>(clockFrequency);
    }

    void Mp34dt05::BitsCaptured(hal::PdmInput::Bits bits)
    {
        really_assert(converter.MaxSamples(bits.size() * bitsPerByte) <= periodStorage.size());

        const auto samples = DiscardStartup(infra::Head(periodStorage, converter.Convert(bits, periodStorage)));

        if (!samples.empty())
        {
            auto callback = onSamples;
            callback(samples);
        }
    }

    infra::MemoryRange<int16_t> Mp34dt05::DiscardStartup(infra::MemoryRange<int16_t> samples)
    {
        const std::size_t discarded = std::min(startupSamplesToDiscard, samples.size());
        startupSamplesToDiscard -= discarded;

        return infra::DiscardHead(samples, discarded);
    }

    void Mp34dt05::Overrun()
    {
        auto callback = onOverrun;
        callback();
    }
}
