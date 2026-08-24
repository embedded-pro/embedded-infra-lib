#include "hal/cortex_m/FaultTracer.hpp"
#include <cstdlib>

namespace
{
    constexpr uintptr_t hfsrAddr = 0xE000ED2Cu;
    constexpr uintptr_t cfsrAddr = 0xE000ED28u;
    constexpr uintptr_t dfsrAddr = 0xE000ED30u;
    constexpr uintptr_t mmfarAddr = 0xE000ED34u;
    constexpr uintptr_t bfarAddr = 0xE000ED38u;

    uint32_t Reg(uintptr_t address)
    {
        return *reinterpret_cast<volatile uint32_t*>(address);
    }

    constexpr uint32_t excReturnFpuMsk = 1u << 4;

    constexpr uint32_t hfsrVecttbl = 1u << 1;
    constexpr uint32_t hfsrForced = 1u << 30;
    constexpr uint32_t hfsrDebugEvt = 1u << 31;

    constexpr uint32_t cfsrIaccviol = 1u << 0;
    constexpr uint32_t cfsrDaccviol = 1u << 1;
    constexpr uint32_t cfsrMunstkerr = 1u << 3;
    constexpr uint32_t cfsrMstkerr = 1u << 4;
    constexpr uint32_t cfsrMlsperr = 1u << 5;
    constexpr uint32_t cfsrMmarvalid = 1u << 7;

    constexpr uint32_t cfsrIbuserr = 1u << 8;
    constexpr uint32_t cfsrPreciserr = 1u << 9;
    constexpr uint32_t cfsrImpreciserr = 1u << 10;
    constexpr uint32_t cfsrUnstkerr = 1u << 11;
    constexpr uint32_t cfsrStkerr = 1u << 12;
    constexpr uint32_t cfsrLsperr = 1u << 13;
    constexpr uint32_t cfsrBfarvalid = 1u << 15;

    constexpr uint32_t cfsrUndefinstr = 1u << 16;
    constexpr uint32_t cfsrInvstate = 1u << 17;
    constexpr uint32_t cfsrInvpc = 1u << 18;
    constexpr uint32_t cfsrNocp = 1u << 19;
    constexpr uint32_t cfsrUnaligned = 1u << 24;
    constexpr uint32_t cfsrDivbyzero = 1u << 25;

    constexpr std::size_t baseFrameWords = 8;
    constexpr std::size_t fpuFrameWords = 18;
    constexpr uint32_t xpsrStackAlignedMsk = 1u << 9;

    enum FrameOffset
    {
        r0 = 0,
        r1 = 1,
        r2 = 2,
        r3 = 3,
        r12 = 4,
        lr = 5,
        pc = 6,
        xpsr = 7,
    };
}

namespace hal::cortex
{
    FaultContext faultContext{};

    FaultTracer::FaultTracer(infra::ConstByteRange codeRange,
        const uint32_t* stackTop,
        services::Tracer& tracer,
        infra::Function<void()> onProgress)
        : codeRange(codeRange)
        , stackTop(stackTop)
        , tracer(tracer)
        , onProgress(onProgress)
    {}

    void FaultTracer::Dump(infra::BoundedConstString faultName)
    {
        tracer.Trace() << "*** Fault: " << faultName << " ***";
        Progress();

        DumpFrame();
        Progress();

#if !defined(__ARM_ARCH_6M__)
        DumpFsrFar();
        Progress();
#endif

        DumpBacktrace();
    }

    void FaultTracer::DumpFrame()
    {
        if (faultContext.stack == nullptr)
        {
            tracer.Trace() << "No fault context available";
            return;
        }

        tracer.Trace() << "Stacked frame:";
        TraceRegister("R0  ", faultContext.stack[r0]);
        TraceRegister("R1  ", faultContext.stack[r1]);
        TraceRegister("R2  ", faultContext.stack[r2]);
        TraceRegister("R3  ", faultContext.stack[r3]);
        TraceRegister("R12 ", faultContext.stack[r12]);
        TraceRegister("LR  ", faultContext.stack[lr]);
        TraceRegister("PC  ", faultContext.stack[pc]);
        TraceRegister("xPSR", faultContext.stack[xpsr]);
        TraceRegister("EXC_RETURN", faultContext.excReturn);

        if ((faultContext.excReturn & excReturnFpuMsk) == 0)
            tracer.Trace() << "  Floating point context stacked";
    }

    void FaultTracer::DumpFsrFar()
    {
        auto hfsr = Reg(hfsrAddr);
        auto cfsr = Reg(cfsrAddr);

        tracer.Trace() << "FSR/FAR:";
        TraceRegister("HFSR", hfsr);

        if ((hfsr & hfsrVecttbl) != 0)
            tracer.Trace() << "    VECTTBL: vector table read fault";
        if ((hfsr & hfsrForced) != 0)
            tracer.Trace() << "    FORCED: escalated from a configurable fault";
        if ((hfsr & hfsrDebugEvt) != 0)
            tracer.Trace() << "    DEBUGEVT: debug event";

        TraceRegister("CFSR", cfsr);
        DumpCfsr(cfsr);
        TraceRegister("DFSR", Reg(dfsrAddr));

        if ((cfsr & cfsrMmarvalid) != 0)
            TraceRegister("MMFAR", Reg(mmfarAddr));
        if ((cfsr & cfsrBfarvalid) != 0)
            TraceRegister("BFAR", Reg(bfarAddr));
    }

    void FaultTracer::DumpCfsr(uint32_t cfsr)
    {
        if ((cfsr & cfsrIaccviol) != 0)
            tracer.Trace() << "    MemManage: instruction access violation";
        if ((cfsr & cfsrDaccviol) != 0)
            tracer.Trace() << "    MemManage: data access violation";
        if ((cfsr & cfsrMunstkerr) != 0)
            tracer.Trace() << "    MemManage: unstack on exception return";
        if ((cfsr & cfsrMstkerr) != 0)
            tracer.Trace() << "    MemManage: stack on exception entry";
        if ((cfsr & cfsrMlsperr) != 0)
            tracer.Trace() << "    MemManage: floating point lazy state preservation";

        if ((cfsr & cfsrIbuserr) != 0)
            tracer.Trace() << "    BusFault: instruction bus error";
        if ((cfsr & cfsrPreciserr) != 0)
            tracer.Trace() << "    BusFault: precise data bus error";
        if ((cfsr & cfsrImpreciserr) != 0)
            tracer.Trace() << "    BusFault: imprecise data bus error";
        if ((cfsr & cfsrUnstkerr) != 0)
            tracer.Trace() << "    BusFault: unstack on exception return";
        if ((cfsr & cfsrStkerr) != 0)
            tracer.Trace() << "    BusFault: stack on exception entry";
        if ((cfsr & cfsrLsperr) != 0)
            tracer.Trace() << "    BusFault: floating point lazy state preservation";

        if ((cfsr & cfsrUndefinstr) != 0)
            tracer.Trace() << "    UsageFault: undefined instruction";
        if ((cfsr & cfsrInvstate) != 0)
            tracer.Trace() << "    UsageFault: invalid state";
        if ((cfsr & cfsrInvpc) != 0)
            tracer.Trace() << "    UsageFault: invalid PC load";
        if ((cfsr & cfsrNocp) != 0)
            tracer.Trace() << "    UsageFault: coprocessor absent or disabled";
        if ((cfsr & cfsrUnaligned) != 0)
            tracer.Trace() << "    UsageFault: unaligned access";
        if ((cfsr & cfsrDivbyzero) != 0)
            tracer.Trace() << "    UsageFault: divide by zero";
    }

    void FaultTracer::DumpBacktrace()
    {
        if (faultContext.stack == nullptr)
            return;

        tracer.Trace() << "Backtrace (code addresses on stack):";

        for (const uint32_t* entry = FrameEnd(); entry < stackTop; ++entry)
        {
            auto candidate = reinterpret_cast<const uint8_t*>(*entry & ~1u);

            if (codeRange.contains(candidate))
                tracer.Continue() << " 0x" << infra::hex << infra::Width(8, '0') << *entry;

            Progress();
        }
    }

    void FaultTracer::TraceRegister(infra::BoundedConstString name, uint32_t value)
    {
        tracer.Trace() << "  " << name << " 0x" << infra::hex << infra::Width(8, '0') << value;
    }

    const uint32_t* FaultTracer::FrameEnd() const
    {
        auto words = baseFrameWords;

        if ((faultContext.excReturn & excReturnFpuMsk) == 0)
            words += fpuFrameWords;

        if ((faultContext.stack[xpsr] & xpsrStackAlignedMsk) != 0)
            ++words;

        return faultContext.stack + words;
    }

    void FaultTracer::Progress()
    {
        if (onProgress)
            onProgress();
    }
}

namespace
{
    [[noreturn]] void Report(infra::BoundedConstString faultName)
    {
        if (hal::cortex::FaultTracer::InstanceSet())
            hal::cortex::FaultTracer::Instance().Dump(faultName);

        std::abort();
    }
}

extern "C"
{
    [[noreturn]] void HardFaultEntry(const uint32_t* stack, uint32_t excReturn)
    {
        hal::cortex::faultContext = { stack, excReturn };
        Report("HardFault");
    }

    [[noreturn]] void MemManageFaultEntry(const uint32_t* stack, uint32_t excReturn)
    {
        hal::cortex::faultContext = { stack, excReturn };
        Report("MemManageFault");
    }

    [[noreturn]] void BusFaultEntry(const uint32_t* stack, uint32_t excReturn)
    {
        hal::cortex::faultContext = { stack, excReturn };
        Report("BusFault");
    }

    [[noreturn]] void UsageFaultEntry(const uint32_t* stack, uint32_t excReturn)
    {
        hal::cortex::faultContext = { stack, excReturn };
        Report("UsageFault");
    }
}

#if defined(__ARM_ARCH_6M__)
#define EMIL_FAULT_HANDLER(name, entry)              \
    extern "C" [[gnu::weak, gnu::naked]] void name() \
    {                                                \
        __asm volatile(                              \
            "movs r0, #4            \n"              \
            "mov  r1, lr            \n"              \
            "tst  r1, r0            \n"              \
            "beq  1f                \n"              \
            "mrs  r0, psp           \n"              \
            "b    2f                \n"              \
            "1:                     \n"              \
            "mrs  r0, msp           \n"              \
            "2:                     \n"              \
            "bl   " #entry "        \n");            \
    }
#else
#define EMIL_FAULT_HANDLER(name, entry)              \
    extern "C" [[gnu::weak, gnu::naked]] void name() \
    {                                                \
        __asm volatile(                              \
            "tst   lr, #4           \n"              \
            "ite   eq               \n"              \
            "mrseq r0, msp          \n"              \
            "mrsne r0, psp          \n"              \
            "mov   r1, lr           \n"              \
            "bl    " #entry "       \n");            \
    }
#endif

EMIL_FAULT_HANDLER(HardFault_Handler, HardFaultEntry)
EMIL_FAULT_HANDLER(MemManage_Handler, MemManageFaultEntry)
EMIL_FAULT_HANDLER(BusFault_Handler, BusFaultEntry)
EMIL_FAULT_HANDLER(UsageFault_Handler, UsageFaultEntry)

#undef EMIL_FAULT_HANDLER
