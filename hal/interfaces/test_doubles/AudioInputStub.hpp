#ifndef HAL_AUDIO_INPUT_STUB_HPP
#define HAL_AUDIO_INPUT_STUB_HPP

#include "hal/interfaces/test_doubles/AudioInputMock.hpp"

namespace hal
{
    class AudioInputStub
        : public AudioInputMock
    {
    public:
        AudioInputStub();

        void PeriodCaptured(Samples samples);
        void Overrun();

    private:
        infra::Function<void(Samples)> onSamples;
        infra::Function<void()> onOverrun;
    };
}

#endif // HAL_AUDIO_INPUT_STUB_HPP
