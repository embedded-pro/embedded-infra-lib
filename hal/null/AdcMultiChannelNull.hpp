#ifndef HAL_ADC_MULTI_CHANNEL_NULL_HPP
#define HAL_ADC_MULTI_CHANNEL_NULL_HPP

#include "hal/interfaces/AdcMultiChannel.hpp"
#include "infra/util/MemoryRange.hpp"
#include "infra/util/WithStorage.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace hal
{
    class AdcMultiChannelNull
        : public AdcMultiChannel
    {
    public:
        template<std::size_t Channels>
        using WithChannels = infra::WithStorage<AdcMultiChannelNull, std::array<uint16_t, Channels>>;

        AdcMultiChannelNull(infra::MemoryRange<uint16_t> samples, uint16_t neutralCode);

        void Measure(const infra::Function<void(Samples)>& onDone) override;
        void Stop() override;

    private:
        infra::MemoryRange<uint16_t> samples;
    };
}

#endif
