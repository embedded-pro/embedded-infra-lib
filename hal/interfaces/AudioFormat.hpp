#ifndef HAL_AUDIO_FORMAT_HPP
#define HAL_AUDIO_FORMAT_HPP

#include <cstdint>

namespace hal
{
    // Samples are interleaved: a frame holds one sample per channel
    struct AudioFormat
    {
        uint32_t sampleRate;
        uint8_t channels;

        bool operator==(const AudioFormat& other) const = default;
    };
}

#endif // HAL_AUDIO_FORMAT_HPP
