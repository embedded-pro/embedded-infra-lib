#include "drivers/microphones/mp45dt02/Mp45dt02.hpp"

namespace drivers
{
    Mp45dt02::Mp45dt02(hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage)
        : PdmMicrophone{ { minClockFrequency, maxClockFrequency, startupTimeInMilliseconds }, bitStream, converter, periodStorage }
    {}
}
