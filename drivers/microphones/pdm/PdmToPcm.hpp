#ifndef DRIVERS_MICROPHONES_PDM_PDM_TO_PCM_HPP
#define DRIVERS_MICROPHONES_PDM_PDM_TO_PCM_HPP

#include "infra/util/ByteRange.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstddef>
#include <cstdint>

namespace drivers
{
    // Turns the 1-bit stream of a PDM microphone into interleaved 16-bit PCM. The bits have the layout described by hal::PdmFormat
    class PdmToPcm
    {
    public:
        // The number of clock cycles of the microphone for each PCM frame
        virtual uint16_t Decimation() const = 0;

        // Forgets the history of the previous stream and prepares for a stream with the given number of channels
        virtual void Reset(uint8_t channels, uint32_t sampleRate) = 0;

        // The most samples that Convert can produce from the next bitCount bits, including the samples of every channel
        virtual std::size_t MaxSamples(std::size_t bitCount) const = 0;

        // Returns the number of samples written. Bits that do not complete a frame are kept for the next call
        virtual std::size_t Convert(infra::ConstByteRange bits, infra::MemoryRange<int16_t> samples) = 0;

    protected:
        PdmToPcm() = default;
        PdmToPcm(const PdmToPcm& other) = delete;
        PdmToPcm& operator=(const PdmToPcm& other) = delete;
        ~PdmToPcm() = default;
    };
}

#endif
