#include "hal/null/SynchronousQuadratureEncoderNull.hpp"

namespace hal
{
    uint32_t SynchronousQuadratureEncoderNull::Position()
    {
        return 0;
    }

    uint32_t SynchronousQuadratureEncoderNull::Resolution()
    {
        return 0;
    }

    SynchronousQuadratureEncoder::MotionDirection SynchronousQuadratureEncoderNull::Direction()
    {
        return MotionDirection::forward;
    }

    uint32_t SynchronousQuadratureEncoderNull::Speed()
    {
        return 0;
    }
}
