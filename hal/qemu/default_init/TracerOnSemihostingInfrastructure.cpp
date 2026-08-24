#include "hal/qemu/default_init/TracerOnSemihostingInfrastructure.hpp"
#include "hal/qemu/cortex/Semihosting.hpp"
#include "services/tracer/GlobalTracer.hpp"

namespace main_
{
    TracerOnSemihostingInfrastructure::TracerOnSemihostingInfrastructure()
        : writer(&hal::cortex::SemihostingWrite)
    {
        services::SetGlobalTracerInstance(tracer);
    }
}
