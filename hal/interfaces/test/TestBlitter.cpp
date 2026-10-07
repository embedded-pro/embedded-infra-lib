#include "hal/interfaces/test_doubles/BlitterStub.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    constexpr hal::DisplaySize regionSize{ 8, 4 };
    constexpr uint32_t stride = 16;

    class BlitterTest
        : public testing::Test
    {
    public:
        hal::Surface Destination()
        {
            return { destination, regionSize, stride, hal::SurfaceFormat::rgb565 };
        }

        hal::ConstSurface Source(hal::SurfaceFormat format = hal::SurfaceFormat::rgb565)
        {
            return { source, regionSize, stride, format };
        }

        infra::Function<void()> Done()
        {
            return [this]()
            {
                ++completions;
            };
        }

        testing::StrictMock<hal::BlitterStub> stub;
        hal::Blitter& blitter{ stub };
        std::array<uint8_t, stride * 4> destination{};
        std::array<uint8_t, stride * 4> source{};
        int completions{ 0 };
    };
}

TEST_F(BlitterTest, support_is_asked_per_operation_and_format)
{
    EXPECT_CALL(stub, Supports(hal::BlitOperation::copy, hal::SurfaceFormat::argb8888, hal::SurfaceFormat::rgb565));
    EXPECT_CALL(stub, Supports(hal::BlitOperation::fill, hal::SurfaceFormat::rgb565, hal::SurfaceFormat::rgb565)).WillOnce(testing::Return(false));

    EXPECT_TRUE(blitter.Supports(hal::BlitOperation::copy, hal::SurfaceFormat::argb8888, hal::SurfaceFormat::rgb565));
    EXPECT_FALSE(blitter.Supports(hal::BlitOperation::fill, hal::SurfaceFormat::rgb565, hal::SurfaceFormat::rgb565));
}

TEST_F(BlitterTest, a_fill_completes_when_the_operation_completes)
{
    EXPECT_CALL(stub, Fill(testing::_, 0xff00ff00, testing::_));

    blitter.Fill(Destination(), 0xff00ff00, Done());

    EXPECT_EQ(0, completions);
    EXPECT_TRUE(stub.OperationPending());

    stub.CompleteOperation();

    EXPECT_EQ(1, completions);
    EXPECT_FALSE(stub.OperationPending());
}

TEST_F(BlitterTest, a_copy_completes_when_the_operation_completes)
{
    EXPECT_CALL(stub, Copy(testing::_, testing::_, testing::_));

    blitter.Copy(Source(), Destination(), Done());

    EXPECT_EQ(0, completions);

    stub.CompleteOperation();

    EXPECT_EQ(1, completions);
}

TEST_F(BlitterTest, a_blend_completes_when_the_operation_completes)
{
    EXPECT_CALL(stub, Blend(testing::_, testing::_, testing::_, testing::_));

    blitter.Blend({ Source(), 128, 0 }, Source(), Destination(), Done());

    EXPECT_EQ(0, completions);

    stub.CompleteOperation();

    EXPECT_EQ(1, completions);
}

TEST_F(BlitterTest, the_next_operation_can_be_started_from_the_completion)
{
    EXPECT_CALL(stub, Fill(testing::_, testing::_, testing::_));
    EXPECT_CALL(stub, Copy(testing::_, testing::_, testing::_));
    blitter.Fill(Destination(), 0, [this]()
        {
            ++completions;
            blitter.Copy(Source(), Destination(), [this]()
                {
                    completions += 10;
                });
        });

    stub.CompleteOperation();
    EXPECT_EQ(1, completions);
    EXPECT_TRUE(stub.OperationPending());

    stub.CompleteOperation();
    EXPECT_EQ(11, completions);
}

TEST_F(BlitterTest, a_second_operation_while_one_is_in_flight_asserts)
{
    EXPECT_CALL(stub, Fill(testing::_, testing::_, testing::_)).Times(testing::AtLeast(1));
    blitter.Fill(Destination(), 0, Done());

    EXPECT_DEATH(blitter.Fill(Destination(), 0, Done()), "");
}

TEST_F(BlitterTest, surfaces_of_different_sizes_assert)
{
    EXPECT_CALL(stub, Copy(testing::_, testing::_, testing::_)).Times(testing::AtLeast(0));
    hal::ConstSurface smaller{ source, { 4, 4 }, stride, hal::SurfaceFormat::rgb565 };

    EXPECT_DEATH(blitter.Copy(smaller, Destination(), Done()), "");
}
