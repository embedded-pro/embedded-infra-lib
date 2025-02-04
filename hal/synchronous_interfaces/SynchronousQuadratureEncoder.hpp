#ifndef HAL_QUADRATURE_ENCODER_HPP
#define HAL_QUADRATURE_ENCODER_HPP

#include "infra/util/Function.hpp"
#include "infra/util/Unit.hpp"

namespace hal
{
    class SynchronousQuadratureEncoder
    {
    public:
        enum class MotionDirection : uint8_t
        {
            forward,
            reverse,
        };

        virtual uint32_t Position() = 0;
        virtual uint32_t Resolution() = 0;
        virtual MotionDirection Direction() = 0;
        virtual uint32_t Speed() = 0;
    };
}

#endif
