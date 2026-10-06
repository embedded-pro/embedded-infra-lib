#include "hal/interfaces/test_doubles/PdmInputStub.hpp"

namespace hal
{
    PdmInputStub::PdmInputStub()
    {
        ON_CALL(*this, Start).WillByDefault([this](PdmFormat, const infra::Function<void(Bits)>& onBits, const infra::Function<void()>& onOverrun)
            {
                this->onBits = onBits;
                this->onOverrun = onOverrun;
            });

        ON_CALL(*this, Stop).WillByDefault([this]()
            {
                onBits = nullptr;
                onOverrun = nullptr;
            });
    }

    void PdmInputStub::PeriodCaptured(Bits bits)
    {
        if (onBits)
        {
            auto callback = onBits;
            callback(bits);
        }
    }

    void PdmInputStub::Overrun()
    {
        if (onOverrun)
        {
            auto callback = onOverrun;
            callback();
        }
    }
}
