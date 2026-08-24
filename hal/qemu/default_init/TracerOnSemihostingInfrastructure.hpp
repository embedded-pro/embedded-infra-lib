#ifndef HAL_QEMU_DEFAULT_INIT_TRACER_ON_SEMIHOSTING_INFRASTRUCTURE_HPP
#define HAL_QEMU_DEFAULT_INIT_TRACER_ON_SEMIHOSTING_INFRASTRUCTURE_HPP

#include "hal/qemu/sync/SemihostingWriter.hpp"
#include "services/tracer/TracerWithDateTime.hpp"

namespace bringup
{
    struct TracerOnSemihostingInfrastructure
    {
        TracerOnSemihostingInfrastructure();

        hal::SemihostingWriter writer;
        infra::TextOutputStream::WithErrorPolicy stream{ writer };
        services::TracerWithDateTime tracer{ stream };
    };
}

#endif
