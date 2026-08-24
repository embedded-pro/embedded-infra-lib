#ifndef HAL_QEMU_DEFAULT_INIT_FAULT_TRACER_INFRASTRUCTURE_HPP
#define HAL_QEMU_DEFAULT_INIT_FAULT_TRACER_INFRASTRUCTURE_HPP

#include "hal/cortex_m/FaultTracer.hpp"

namespace main_
{
    struct FaultTracerInfrastructure
    {
        explicit FaultTracerInfrastructure(services::Tracer& tracer,
            infra::Function<void()> onProgress = infra::Function<void()>());

        FaultTracerInfrastructure(infra::ConstByteRange codeRange,
            const uint32_t* stackTop,
            services::Tracer& tracer,
            infra::Function<void()> onProgress = infra::Function<void()>());

        hal::cortex::FaultTracer faultTracer;
    };
}

#endif
