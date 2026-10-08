#ifndef DRIVERS_MICROPHONES_MP45DT02_MP45DT02_HPP
#define DRIVERS_MICROPHONES_MP45DT02_MP45DT02_HPP

#include "drivers/microphones/pdm/PdmMicrophone.hpp"
#include "drivers/microphones/pdm/PdmToPcm.hpp"
#include "hal/interfaces/AudioInput.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace drivers
{
    class Mp45dt02
        : public PdmMicrophone
    {
    public:
        static constexpr uint32_t minClockFrequency{ 1000000 };
        static constexpr uint32_t maxClockFrequency{ 3250000 };
        static constexpr uint32_t startupTimeInMilliseconds{ 10 };

        Mp45dt02(hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage);
    };
}

#endif
