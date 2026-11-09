#include "hal/null/SynchronousQuadratureEncoderNull.hpp"
#include "gtest/gtest.h"

TEST(SynchronousQuadratureEncoderNullTest, reports_a_shaft_at_rest)
{
    hal::SynchronousQuadratureEncoderNull encoder;

    EXPECT_EQ(0, encoder.Position());
    EXPECT_EQ(0, encoder.Resolution());
    EXPECT_EQ(hal::SynchronousQuadratureEncoder::MotionDirection::forward, encoder.Direction());
    EXPECT_EQ(0, encoder.Speed());
}
