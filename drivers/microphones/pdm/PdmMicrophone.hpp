#ifndef DRIVERS_MICROPHONES_PDM_PDM_MICROPHONE_HPP
#define DRIVERS_MICROPHONES_PDM_PDM_MICROPHONE_HPP

#include "drivers/microphones/pdm/PdmToPcm.hpp"
#include "hal/interfaces/AudioInput.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // The raw stream has one channel per microphone and a sample rate of the clock frequency divided by 16
    class PdmMicrophone
        : public hal::AudioInput
    {
    public:
        struct Limits
        {
            uint32_t minClockFrequency;
            uint32_t maxClockFrequency;
            uint32_t startupTimeInMilliseconds;
        };

        PdmMicrophone(const Limits& limits, hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage);
        ~PdmMicrophone();

        void Start(hal::AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun) override;
        void Stop(const infra::Function<void()>& onStopped) override;

    private:
        uint32_t ClockFrequency(hal::AudioFormat format) const;
        void WordsCaptured(Samples words);
        infra::MemoryRange<int16_t> DiscardStartup(infra::MemoryRange<int16_t> samples);
        void Overrun();

    private:
        const Limits limits;
        hal::AudioInput& bitStream;
        PdmToPcm& converter;
        infra::MemoryRange<int16_t> periodStorage;
        infra::Function<void(Samples)> onSamples;
        infra::Function<void()> onOverrun;
        bool running{ false };
        std::size_t startupSamplesToDiscard{ 0 };
    };
}

#endif
