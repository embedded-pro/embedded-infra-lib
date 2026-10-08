#ifndef HAL_AUDIO_INPUT_HPP
#define HAL_AUDIO_INPUT_HPP

#include "hal/interfaces/AudioFormat.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal
{
    class AudioInput
    {
    public:
        using Samples = infra::MemoryRange<const int16_t>;

        virtual void Start(AudioFormat format, const infra::Function<void(Samples)>& onSamples, const infra::Function<void()>& onOverrun) = 0;
        virtual void Stop(const infra::Function<void()>& onStopped) = 0;

    protected:
        AudioInput() = default;
        AudioInput(const AudioInput& other) = delete;
        AudioInput& operator=(const AudioInput& other) = delete;
        ~AudioInput() = default;
    };
}

#endif // HAL_AUDIO_INPUT_HPP
