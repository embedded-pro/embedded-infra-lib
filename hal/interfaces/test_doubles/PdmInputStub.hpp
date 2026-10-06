#ifndef HAL_PDM_INPUT_STUB_HPP
#define HAL_PDM_INPUT_STUB_HPP

#include "hal/interfaces/test_doubles/PdmInputMock.hpp"

namespace hal
{
    class PdmInputStub
        : public PdmInputMock
    {
    public:
        PdmInputStub();

        void PeriodCaptured(Bits bits);
        void Overrun();

    private:
        infra::Function<void(Bits)> onBits;
        infra::Function<void()> onOverrun;
    };
}

#endif // HAL_PDM_INPUT_STUB_HPP
