#ifndef DRIVERS_MICROPHONES_PDM_TEST_DOUBLES_PDM_TO_PCM_MOCK_HPP
#define DRIVERS_MICROPHONES_PDM_TEST_DOUBLES_PDM_TO_PCM_MOCK_HPP

#include "drivers/microphones/pdm/PdmToPcm.hpp"
#include "gmock/gmock.h"

namespace drivers
{
    class PdmToPcmMock
        : public PdmToPcm
    {
    public:
        MOCK_METHOD(uint16_t, Decimation, (), (const, override));
        MOCK_METHOD(void, Reset, (uint8_t channels, uint32_t sampleRate), (override));
        MOCK_METHOD(std::size_t, MaxSamples, (std::size_t wordCount), (const, override));
        MOCK_METHOD(std::size_t, Convert, (infra::MemoryRange<const int16_t> words, infra::MemoryRange<int16_t> samples), (override));
    };
}

#endif
