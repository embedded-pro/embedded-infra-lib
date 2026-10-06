#include "hal/interfaces/test_doubles/PdmInputStub.hpp"
#include "infra/util/MemoryRange.hpp"
#include "gtest/gtest.h"
#include <algorithm>
#include <array>
#include <cstdint>

namespace
{
    constexpr hal::PdmFormat stereo3072k{ 3072000, 2 };
    constexpr hal::PdmFormat mono2048k{ 2048000, 1 };

    class PdmInputTest
        : public testing::Test
    {
    public:
        void Start(hal::PdmFormat format = stereo3072k)
        {
            EXPECT_CALL(stub, Start(format, testing::_, testing::_));

            input.Start(
                format, [this](hal::PdmInput::Bits bits)
                {
                    ++periodsReceived;
                    received.fill(0);
                    std::copy_n(bits.begin(), std::min(bits.size(), received.size()), received.begin());
                    sizeReceived = bits.size();
                },
                [this]()
                {
                    ++overruns;
                });
        }

        testing::StrictMock<hal::PdmInputStub> stub;
        hal::PdmInput& input{ stub };
        std::array<uint8_t, 8> received{};
        int periodsReceived{ 0 };
        std::size_t sizeReceived{ 0 };
        int overruns{ 0 };
    };
}

TEST_F(PdmInputTest, Start_hands_the_requested_format_to_the_implementation)
{
    Start(mono2048k);
}

TEST_F(PdmInputTest, Stop_halts_the_stream)
{
    EXPECT_CALL(stub, Stop());

    input.Stop();
}

TEST_F(PdmInputTest, no_bits_are_received_before_a_period_is_captured)
{
    Start();

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(PdmInputTest, captured_bits_reach_the_consumer)
{
    Start();

    std::array<uint8_t, 4> captured{ 0x00, 0xff, 0xa5, 0x80 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(std::size_t(4), sizeReceived);
    EXPECT_EQ(0x00, received[0]);
    EXPECT_EQ(0xff, received[1]);
    EXPECT_EQ(0xa5, received[2]);
    EXPECT_EQ(0x80, received[3]);
}

TEST_F(PdmInputTest, consumer_stays_registered_across_periods)
{
    Start();

    std::array<uint8_t, 2> first{ 1, 2 };
    std::array<uint8_t, 2> second{ 3, 4 };
    stub.PeriodCaptured(infra::MakeConstRange(first));
    stub.PeriodCaptured(infra::MakeConstRange(second));

    EXPECT_EQ(2, periodsReceived);
    EXPECT_EQ(3, received[0]);
    EXPECT_EQ(4, received[1]);
}

TEST_F(PdmInputTest, overrun_is_reported_through_the_overrun_callback)
{
    Start();

    stub.Overrun();

    EXPECT_EQ(1, overruns);
    EXPECT_EQ(0, periodsReceived);
}

TEST_F(PdmInputTest, stream_keeps_running_after_an_overrun)
{
    Start();
    stub.Overrun();

    std::array<uint8_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, periodsReceived);
}

TEST_F(PdmInputTest, no_bits_are_received_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    std::array<uint8_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(0, periodsReceived);
}

TEST_F(PdmInputTest, no_overrun_is_reported_after_Stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    stub.Overrun();

    EXPECT_EQ(0, overruns);
}

TEST_F(PdmInputTest, consumer_may_stop_the_stream_from_within_a_period)
{
    EXPECT_CALL(stub, Start(stereo3072k, testing::_, testing::_));
    input.Start(
        stereo3072k, [this](hal::PdmInput::Bits)
        {
            ++periodsReceived;
            input.Stop();
        },
        []() {});

    EXPECT_CALL(stub, Stop());
    std::array<uint8_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, periodsReceived);
}

TEST_F(PdmInputTest, restarting_registers_new_callbacks)
{
    Start();
    EXPECT_CALL(stub, Stop());
    input.Stop();

    int restartedPeriods = 0;
    EXPECT_CALL(stub, Start(mono2048k, testing::_, testing::_));
    input.Start(
        mono2048k, [&restartedPeriods](hal::PdmInput::Bits)
        {
            ++restartedPeriods;
        },
        []() {});
    std::array<uint8_t, 2> captured{ 1, 2 };
    stub.PeriodCaptured(infra::MakeConstRange(captured));

    EXPECT_EQ(1, restartedPeriods);
    EXPECT_EQ(0, periodsReceived);
}

TEST(PdmFormatTest, formats_with_equal_clock_and_channels_are_equal)
{
    EXPECT_EQ((hal::PdmFormat{ 3072000, 2 }), (hal::PdmFormat{ 3072000, 2 }));
}

TEST(PdmFormatTest, formats_differing_in_clock_or_channels_are_not_equal)
{
    EXPECT_NE((hal::PdmFormat{ 3072000, 2 }), (hal::PdmFormat{ 2048000, 2 }));
    EXPECT_NE((hal::PdmFormat{ 3072000, 2 }), (hal::PdmFormat{ 3072000, 1 }));
}
