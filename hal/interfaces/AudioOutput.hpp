#ifndef HAL_AUDIO_OUTPUT_HPP
#define HAL_AUDIO_OUTPUT_HPP

#include "hal/interfaces/AudioFormat.hpp"
#include "infra/util/Function.hpp"
#include "infra/util/MemoryRange.hpp"
#include <cstdint>

namespace hal
{
    class AudioOutput
    {
    public:
        using Samples = infra::MemoryRange<int16_t>;

        virtual void Start(AudioFormat format, const infra::Function<void(Samples toFill)>& onSamplesRequired, const infra::Function<void()>& onUnderrun) = 0;
        virtual void Stop(const infra::Function<void()>& onStopped) = 0;
        virtual void SetVolume(uint8_t percent, const infra::Function<void()>& onDone) = 0;
        virtual void SetMuted(bool muted, const infra::Function<void()>& onDone) = 0;

    protected:
        AudioOutput() = default;
        AudioOutput(const AudioOutput& other) = delete;
        AudioOutput& operator=(const AudioOutput& other) = delete;
        ~AudioOutput() = default;
    };
}

#endif // HAL_AUDIO_OUTPUT_HPP
