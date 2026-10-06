#ifndef HAL_AUDIO_INPUT_MOCK_HPP
#define HAL_AUDIO_INPUT_MOCK_HPP

#include "hal/interfaces/AudioInput.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class AudioInputMock
        : public AudioInput
    {
    public:
        MOCK_METHOD(void, Start, (AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun), (override));
        MOCK_METHOD(void, Stop, (), (override));
    };
}

#endif // HAL_AUDIO_INPUT_MOCK_HPP
