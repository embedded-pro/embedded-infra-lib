#include "hal/qemu/default_init/TracerOnSemihostingInfrastructure.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "services/tracer/GlobalTracer.hpp"

namespace bringup
{
    TracerOnSemihostingInfrastructure::TracerOnSemihostingInfrastructure()
        : writer(&hal::cortex::SemihostingWrite)
    {
        services::SetGlobalTracerInstance(tracer);
    }
}
