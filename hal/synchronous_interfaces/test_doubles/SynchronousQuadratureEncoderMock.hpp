#ifndef HAL_SYNCHRONOUS_QUADRATUE_ENCODER_MOCK_HPP
#define HAL_SYNCHRONOUS_QUADRATUE_ENCODER_MOCK_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class SynchronousQuadratureEncoderMock
        : public SynchronousQuadratureEncoder
    {
    public:
        MOCK_METHOD(uint32_t, Position, ());
        MOCK_METHOD(uint32_t, Resolution, ());
        MOCK_METHOD(MotionDirection, Direction, ());
        MOCK_METHOD(uint32_t, Speed, ());
    };
}

#endif
