#ifndef HAL_AUDIO_OUTPUT_STUB_HPP
#define HAL_AUDIO_OUTPUT_STUB_HPP

#include "hal/interfaces/test_doubles/AudioOutputMock.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace hal
{
    // Stop, SetVolume and SetMuted complete immediately
    class AudioOutputStub
        : public AudioOutputMock
    {
    public:
        explicit AudioOutputStub(infra::MemoryRange<int16_t> storage);

        void PeriodElapsed(std::size_t numberOfSamples);
        void Underrun();

        infra::MemoryRange<const int16_t> LastPeriod() const;
        uint8_t Volume() const;
        bool Muted() const;

        template<std::size_t StorageSize>
        using WithStorage = infra::WithStorage<AudioOutputStub, std::array<int16_t, StorageSize>>;

    private:
        infra::MemoryRange<int16_t> storage;
        std::size_t lastPeriodSize{ 0 };
        uint8_t volume{ 100 };
        bool muted{ false };
        infra::Function<void(Samples)> onSamplesRequired;
        infra::Function<void()> onUnderrun;
    };
}

#endif // HAL_AUDIO_OUTPUT_STUB_HPP
