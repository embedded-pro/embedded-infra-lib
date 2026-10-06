#ifndef HAL_AUDIO_OUTPUT_MOCK_HPP
#define HAL_AUDIO_OUTPUT_MOCK_HPP

#include "hal/interfaces/AudioOutput.hpp"
#include "gmock/gmock.h"

namespace hal
{
    class AudioOutputMock
        : public AudioOutput
    {
    public:
        MOCK_METHOD(void, Start, (AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun), (override));
        MOCK_METHOD(void, Stop, (), (override));
        MOCK_METHOD(void, SetVolume, (uint8_t percent), (override));
        MOCK_METHOD(void, SetMuted, (bool muted), (override));
    };
}

#endif // HAL_AUDIO_OUTPUT_MOCK_HPP
