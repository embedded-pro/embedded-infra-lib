#include "hal/cortex_m/FaultTracer.hpp"
#include "infra/stream/StringOutputStream.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <utility>

namespace
{
    constexpr uint32_t codeRangeBegin = 0x00001000;
    constexpr uint32_t codeRangeEnd = 0x00002000;

    constexpr uint32_t returnAddressInCode = 0x00001234;
    constexpr uint32_t thumbReturnAddressInCode = 0x00001445;
    constexpr uint32_t addressOutsideCode = 0x20004000;

    constexpr uint32_t excReturnNoFpu = 0xFFFFFFF9;
    constexpr uint32_t excReturnWithFpu = 0xFFFFFFE9;
}

class FaultTracerTest
    : public testing::Test
{
public:
    FaultTracerTest()
    {
        frame.fill(0);
        hal::cortex::faultContext = { frame.data(), excReturnNoFpu };
    }

    ~FaultTracerTest() override
    {
        hal::cortex::faultContext = { nullptr, 0 };
    }

    hal::cortex::FaultTracer Construct()
    {
        return hal::cortex::FaultTracer(
            infra::ConstByteRange(reinterpret_cast<const uint8_t*>(codeRangeBegin),
                reinterpret_cast<const uint8_t*>(codeRangeEnd)),
            frame.data() + frame.size(), tracer);
    }

    bool Traced(const char* text)
    {
        return stream.Storage().find(text) != infra::BoundedString::npos;
    }

    infra::StringOutputStream::WithStorage<2048> stream;
    services::TracerToStream tracer{ stream };

    std::array<uint32_t, 32> frame{};
};

TEST_F(FaultTracerTest, dump_reports_the_fault_name)
{
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("*** Fault: HardFault ***"));
}

TEST_F(FaultTracerTest, dump_reports_the_stacked_registers)
{
    frame[0] = 0x11111111;
    frame[1] = 0x22222222;
    frame[2] = 0x33333333;
    frame[3] = 0x44444444;
    frame[4] = 0x55555555;
    frame[5] = 0x66666666;
    frame[6] = 0x77777777;
    frame[7] = 0x00000000;
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("R0   0x11111111"));
    EXPECT_TRUE(Traced("R1   0x22222222"));
    EXPECT_TRUE(Traced("R2   0x33333333"));
    EXPECT_TRUE(Traced("R3   0x44444444"));
    EXPECT_TRUE(Traced("R12  0x55555555"));
    EXPECT_TRUE(Traced("LR   0x66666666"));
    EXPECT_TRUE(Traced("PC   0x77777777"));
    EXPECT_TRUE(Traced("xPSR 0x00000000"));
}

TEST_F(FaultTracerTest, dump_reports_the_exception_return_value)
{
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("EXC_RETURN 0xfffffff9"));
}

TEST_F(FaultTracerTest, a_stacked_floating_point_context_is_reported)
{
    hal::cortex::faultContext = { frame.data(), excReturnWithFpu };
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("Floating point context stacked"));
}

TEST_F(FaultTracerTest, an_absent_floating_point_context_is_not_reported)
{
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_FALSE(Traced("Floating point context stacked"));
}

TEST_F(FaultTracerTest, dump_reports_when_no_fault_context_was_captured)
{
    hal::cortex::faultContext = { nullptr, 0 };
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("No fault context available"));
}

TEST_F(FaultTracerTest, the_backtrace_reports_addresses_inside_the_code_range)
{
    frame[8] = returnAddressInCode;
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("Backtrace (code addresses on stack):"));
    EXPECT_TRUE(Traced("0x00001234"));
}

TEST_F(FaultTracerTest, the_backtrace_skips_addresses_outside_the_code_range)
{
    frame[8] = addressOutsideCode;
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_FALSE(Traced("0x20004000"));
}

TEST_F(FaultTracerTest, the_backtrace_masks_the_thumb_bit_when_matching_the_code_range)
{
    frame[8] = thumbReturnAddressInCode;
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("0x00001445"));
}

TEST_F(FaultTracerTest, the_backtrace_skips_the_exception_frame_it_already_reported)
{
    frame[6] = returnAddressInCode;
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    auto storage = stream.Storage();
    auto backtrace = storage.substr(storage.find("Backtrace"));
    EXPECT_EQ(infra::BoundedString::npos, backtrace.find("0x00001234"));
}

TEST_F(FaultTracerTest, on_progress_is_invoked_while_dumping)
{
    uint32_t progressCount = 0;
    hal::cortex::FaultTracer faultTracer(
        infra::ConstByteRange(reinterpret_cast<const uint8_t*>(codeRangeBegin),
            reinterpret_cast<const uint8_t*>(codeRangeEnd)),
        frame.data() + frame.size(), tracer, [&progressCount]()
        {
            ++progressCount;
        });

    faultTracer.Dump("HardFault");

    auto scannedWords = frame.size() - 8;
#if defined(__ARM_ARCH_6M__)
    EXPECT_EQ(2u + scannedWords, progressCount);
#else
    EXPECT_EQ(3u + scannedWords, progressCount);
#endif
}

TEST_F(FaultTracerTest, dump_without_on_progress_does_not_crash)
{
    auto faultTracer = Construct();

    faultTracer.Dump("UsageFault");

    EXPECT_TRUE(Traced("*** Fault: UsageFault ***"));
}

#if !defined(__ARM_ARCH_6M__)

TEST_F(FaultTracerTest, dump_reports_the_status_registers)
{
    auto faultTracer = Construct();

    faultTracer.Dump("HardFault");

    EXPECT_TRUE(Traced("FSR/FAR:"));
    EXPECT_TRUE(Traced("HFSR 0x"));
    EXPECT_TRUE(Traced("CFSR 0x"));
    EXPECT_TRUE(Traced("DFSR 0x"));
}

#endif

class CfsrDecodeTest
    : public FaultTracerTest
    , public testing::WithParamInterface<std::pair<uint32_t, const char*>>
{};

TEST_P(CfsrDecodeTest, the_status_bit_is_decoded)
{
    auto faultTracer = Construct();

    faultTracer.DumpCfsr(GetParam().first);

    EXPECT_TRUE(Traced(GetParam().second));
}

TEST_F(FaultTracerTest, an_empty_cfsr_decodes_to_nothing)
{
    auto faultTracer = Construct();

    faultTracer.DumpCfsr(0);

    EXPECT_TRUE(stream.Storage().empty());
}

INSTANTIATE_TEST_SUITE_P(MemManage, CfsrDecodeTest,
    testing::Values(
        std::make_pair(1u << 0, "MemManage: instruction access violation"),
        std::make_pair(1u << 1, "MemManage: data access violation"),
        std::make_pair(1u << 3, "MemManage: unstack on exception return"),
        std::make_pair(1u << 4, "MemManage: stack on exception entry"),
        std::make_pair(1u << 5, "MemManage: floating point lazy state preservation")));

INSTANTIATE_TEST_SUITE_P(BusFault, CfsrDecodeTest,
    testing::Values(
        std::make_pair(1u << 8, "BusFault: instruction bus error"),
        std::make_pair(1u << 9, "BusFault: precise data bus error"),
        std::make_pair(1u << 10, "BusFault: imprecise data bus error"),
        std::make_pair(1u << 11, "BusFault: unstack on exception return"),
        std::make_pair(1u << 12, "BusFault: stack on exception entry"),
        std::make_pair(1u << 13, "BusFault: floating point lazy state preservation")));

INSTANTIATE_TEST_SUITE_P(UsageFault, CfsrDecodeTest,
    testing::Values(
        std::make_pair(1u << 16, "UsageFault: undefined instruction"),
        std::make_pair(1u << 17, "UsageFault: invalid state"),
        std::make_pair(1u << 18, "UsageFault: invalid PC load"),
        std::make_pair(1u << 19, "UsageFault: coprocessor absent or disabled"),
        std::make_pair(1u << 24, "UsageFault: unaligned access"),
        std::make_pair(1u << 25, "UsageFault: divide by zero")));
