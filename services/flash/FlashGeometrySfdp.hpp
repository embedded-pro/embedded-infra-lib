#ifndef SERVICES_FLASH_GEOMETRY_SFDP_HPP
#define SERVICES_FLASH_GEOMETRY_SFDP_HPP

#include "hal/interfaces/Spi.hpp"
#include "services/flash/FlashGeometrySfdpBase.hpp"
#include <array>

namespace services
{
    class FlashGeometrySfdp
        : public FlashGeometrySfdpBase
    {
    public:
        FlashGeometrySfdp(hal::SpiMaster& spi, infra::Function<void()> onInitialized);

    private:
        void PerformRead(uint32_t address, infra::ByteRange buffer, infra::Function<void()> onDone) override;

        hal::SpiMaster& spi;
        std::array<uint8_t, 5> commandBuffer{};
        infra::ByteRange pendingBuffer;
        infra::Function<void()> pendingOnDone;
    };
}

#endif
