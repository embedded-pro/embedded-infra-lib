#include "drivers/microphones/mp34dt05/Mp34dt05.hpp"

namespace drivers
{
    Mp34dt05::Mp34dt05(hal::AudioInput& bitStream, PdmToPcm& converter, infra::MemoryRange<int16_t> periodStorage)
        : PdmMicrophone{ { minClockFrequency, maxClockFrequency, startupTimeInMilliseconds }, bitStream, converter, periodStorage }
    {}
}
