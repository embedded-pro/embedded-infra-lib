#ifndef HAL_CORTEX_M_RESET_HPP
#define HAL_CORTEX_M_RESET_HPP

#include "hal/interfaces/Reset.hpp"

namespace hal::cortex
{
    class Reset
        : public hal::Reset
    {
    public:
        void ResetModule(const char* resetReason) override;
    };
}

#endif
