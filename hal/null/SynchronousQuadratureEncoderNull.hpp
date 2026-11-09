#ifndef HAL_SYNCHRONOUS_QUADRATURE_ENCODER_NULL_HPP
#define HAL_SYNCHRONOUS_QUADRATURE_ENCODER_NULL_HPP

#include "hal/synchronous_interfaces/SynchronousQuadratureEncoder.hpp"
#include <cstdint>

namespace hal
{
    class SynchronousQuadratureEncoderNull
        : public SynchronousQuadratureEncoder
    {
    public:
        uint32_t Position() override;
        uint32_t Resolution() override;
        MotionDirection Direction() override;
        uint32_t Speed() override;
    };
}

#endif
