#ifndef HAL_PDM_INPUT_MOCK_HPP
#define HAL_PDM_INPUT_MOCK_HPP

#include "hal/interfaces/PdmInput.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class PdmInputMock
        : public PdmInput
    {
    public:
        MOCK_METHOD(void, Start, (PdmFormat format, const infra::Function<void(Bits)>& onBits, const infra::Function<void()>& onOverrun), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_PDM_INPUT_MOCK_HPP
