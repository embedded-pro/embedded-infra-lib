#ifndef DRIVERS_MICROPHONES_MP34DT05_MP34DT05_HPP
#define DRIVERS_MICROPHONES_MP34DT05_MP34DT05_HPP

#include "drivers/microphones/pdm/PdmMicrophone.hpp"
#include "drivers/microphones/pdm/PdmToPcm.hpp"
#include "hal/interfaces/AudioInput.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace drivers
{
    class Mp34dt05
        : public PdmMicrophone
    {
    public:
        static constexpr uint32_t minClockFrequency{ 1280000 };
        static constexpr uint32_t maxClockFrequency{ 3250000 };
        static constexpr uint32_t startupTimeInMilliseconds{ 20 };

        Mp34dt05(hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage);
    };
}

#endif
