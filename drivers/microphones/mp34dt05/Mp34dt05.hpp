#ifndef DRIVERS_MICROPHONES_MP34DT05_MP34DT05_HPP
#define DRIVERS_MICROPHONES_MP34DT05_MP34DT05_HPP

#include "drivers/microphones/pdm/PdmToPcm.hpp"
#include "hal/interfaces/AudioInput.hpp"
#include "hal/interfaces/PdmInput.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // The microphone has no control interface: the driver clocks it through a hal::PdmInput and hands the bits to a PdmToPcm.
    // Two microphones that share a data line are a stereo stream, see hal::PdmFormat
    class Mp34dt05
        : public hal::AudioInput
    {
    public:
        static constexpr uint32_t minClockFrequency{ 1280000 };
        static constexpr uint32_t maxClockFrequency{ 3250000 };
        static constexpr uint32_t startupTimeInMilliseconds{ 20 };

        // periodStorage must hold the samples that converter produces from one period of the input
        Mp34dt05(hal::PdmInput& input, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage);
        ~Mp34dt05();

        void Start(hal::AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun) override;
        void Stop() override;

    private:
        uint32_t ClockFrequency(hal::AudioFormat format) const;
        void BitsCaptured(hal::PdmInput::Bits bits);
        infra::MemoryRange<int16_t> DiscardStartup(infra::MemoryRange<int16_t> samples);
        void Overrun();

    private:
        hal::PdmInput& input;
        PdmToPcm& converter;
        infra::MemoryRange<int16_t> periodStorage;
        infra::Function<void(Samples)> onSamples;
        infra::Function<void()> onOverrun;
        bool running{ false };
        std::size_t startupSamplesToDiscard{ 0 };
    };
}

#endif
