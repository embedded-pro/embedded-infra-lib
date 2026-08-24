#ifndef HAL_CORTEX_M_FAULT_TRACER_HPP
#define HAL_CORTEX_M_FAULT_TRACER_HPP

#include "infra/util/BoundedString.hpp"
#include "infra/util/ByteRange.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/InterfaceConnector.hpp"
#include "services/tracer/Tracer.hpp"
#include <cstdint>

namespace hal::cortex
{
    // Captured by the fault entry stubs before any C++ runs.
    struct FaultContext
    {
        const uint32_t* stack;
        uint32_t excReturn;
    };

    extern FaultContext faultContext;

    class FaultTracer
        : public infra::InterfaceConnector<FaultTracer>
    {
    public:
        FaultTracer(infra::ConstByteRange codeRange,
            const uint32_t* stackTop,
            services::Tracer& tracer,
            infra::Function<void()> onProgress = infra::Function<void()>());

        // Writes the fault report without terminating, so that the report can be
        // inspected by tests; the fault entry stubs abort once this returns.
        void Dump(infra::BoundedConstString faultName);

        // Decodes a Configurable Fault Status Register value. Public because the value is
        // meaningful on its own, for instance when one has been latched for later report.
        void DumpCfsr(uint32_t cfsr);

    private:
        void DumpFrame();
        void DumpFsrFar();
        void DumpBacktrace();

        void TraceRegister(infra::BoundedConstString name, uint32_t value);
        const uint32_t* FrameEnd() const;
        void Progress();

    private:
        infra::ConstByteRange codeRange;
        const uint32_t* stackTop;
        services::Tracer& tracer;
        infra::Function<void()> onProgress;
    };
}

#endif
