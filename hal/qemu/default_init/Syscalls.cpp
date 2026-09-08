#include "hal/cortex_m/FaultTracer.hpp"
#include "hal/cortex_m/Semihosting.hpp"
#include "hal/qemu/default_init/FaultTracerInfrastructure.hpp"
#include "hal/qemu/sync/SemihostingWriter.hpp"
#include "infra/util/ByteRange.hpp"
#include "services/tracer/GlobalTracer.hpp"
#include <array>
#include <cstdint>

namespace
{
    void TraceAbort(const uint32_t* stackPointer, uint32_t linkRegister)
    {
        if (hal::cortex::FaultTracer::InstanceSet())
        {
            hal::cortex::FaultTracer::Instance().DumpAbort(stackPointer, linkRegister);
            return;
        }

        hal::SemihostingWriter writer{ &hal::cortex::SemihostingWrite };
        infra::TextOutputStream::WithErrorPolicy stream{ writer };
        services::TracerToStream tracerOnSemihosting{ stream };

        bringup::FaultTracerInfrastructure infrastructure{
            services::GlobalTracerSet() ? services::GlobalTracer() : tracerOnSemihosting
        };
        infrastructure.faultTracer.DumpAbort(stackPointer, linkRegister);
    }
}

extern "C" int _write(int, const char* buf, int count)
{
    hal::cortex::SemihostingWrite(infra::ConstByteRange(
        reinterpret_cast<const uint8_t*>(buf),
        reinterpret_cast<const uint8_t*>(buf) + count));
    return count;
}

extern "C" [[noreturn]] void abort()
{
    TraceAbort(static_cast<const uint32_t*>(__builtin_frame_address(0)),
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(__builtin_return_address(0))));

    static std::array<uint32_t, 2> exitBlock{ 0x20026u, 1u };
    hal::cortex::SemihostingCall(hal::cortex::SemihostingOperation::exitExtended, exitBlock.data());

    while (true)
    {}
}
