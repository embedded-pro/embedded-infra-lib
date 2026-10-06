#ifndef DRIVERS_MICROPHONES_PDM_PDM_TO_PCM_HPP
#define DRIVERS_MICROPHONES_PDM_PDM_TO_PCM_HPP

#include "infra/util/MemoryRange.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // The input is the raw stream of a hal::AudioInput: a channel is a microphone, and each 16-bit word holds the next 16 clock cycles of that microphone, the first in the most significant bit
    class PdmToPcm
    {
    public:
        virtual uint16_t Decimation() const = 0;
        virtual void Reset(uint8_t channels, uint32_t sampleRate) = 0;

        // Counts the samples of every channel
        virtual std::size_t MaxSamples(std::size_t wordCount) const = 0;

        // Bits that do not complete a frame are kept for the next call
        virtual std::size_t Convert(infra::MemoryRange<const int16_t> words, infra::MemoryRange<int16_t> samples) = 0;

    protected:
        PdmToPcm() = default;
        PdmToPcm(const PdmToPcm& other) = delete;
        PdmToPcm& operator=(const PdmToPcm& other) = delete;
        ~PdmToPcm() = default;
    };
}

#endif
