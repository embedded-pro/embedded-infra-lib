#ifndef HAL_PDM_INPUT_HPP
#define HAL_PDM_INPUT_HPP

#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal
{
    // The bit rate is clockFrequency * channels. Bits are in time order, most significant bit first.
    // With two channels the bits come in pairs: channel 0 is sampled on the rising clock edge, channel 1 on the falling clock edge
    struct PdmFormat
    {
        uint32_t clockFrequency;
        uint8_t channels;

        bool operator==(const PdmFormat& other) const = default;
    };

    class PdmInput
    {
    public:
        using Bits = infra::MemoryRange<const uint8_t>;

        virtual void Start(PdmFormat format, const infra::Function<void(Bits)>& onBits, const infra::Function<void()>& onOverrun) = 0;
        virtual void Stop() = 0;

    protected:
        PdmInput() = default;
        PdmInput(const PdmInput& other) = delete;
        PdmInput& operator=(const PdmInput& other) = delete;
        ~PdmInput() = default;
    };
}

#endif // HAL_PDM_INPUT_HPP
