#include "hal/qemu/default_init/FaultTracerInfrastructure.hpp"

extern uint32_t _estack;
extern uint32_t _stext;
extern uint32_t _etext;

namespace main_
{
    FaultTracerInfrastructure::FaultTracerInfrastructure(services::Tracer& tracer,
        infra::Function<void()> onProgress)
        : FaultTracerInfrastructure(infra::ConstByteRange(reinterpret_cast<const uint8_t*>(&_stext),
                                        reinterpret_cast<const uint8_t*>(&_etext)),
              &_estack, tracer, onProgress)
    {}

    FaultTracerInfrastructure::FaultTracerInfrastructure(infra::ConstByteRange codeRange,
        const uint32_t* stackTop,
        services::Tracer& tracer,
        infra::Function<void()> onProgress)
        : faultTracer(codeRange, stackTop, tracer, onProgress)
    {}
}
