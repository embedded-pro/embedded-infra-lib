#ifndef HAL_BLITTER_STUB_HPP
#define HAL_BLITTER_STUB_HPP

#include "hal/interfaces/test_doubles/BlitterMock.hpp"
#include "infra/util/AutoResetFunction.hpp"

namespace hal
{
    // Checks the contract of a blitter and completes an operation when the test says so. It does not touch pixels
    class BlitterStub
        : public BlitterMock
    {
    public:
        BlitterStub();

        void CompleteOperation();
        bool OperationPending() const;

    private:
        void Begin(const infra::Function<void()>& onDone);

    private:
        infra::AutoResetFunction<void()> pendingCompletion;
    };
}

#endif
