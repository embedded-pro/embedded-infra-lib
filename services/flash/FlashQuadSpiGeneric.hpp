#ifndef SERVICES_FLASH_QUAD_SPI_GENERIC_HPP
#define SERVICES_FLASH_QUAD_SPI_GENERIC_HPP

#include "services/flash/FlashGeometryQuad.hpp"
#include "services/flash/FlashQuadSpi.hpp"

namespace services
{
    class FlashQuadSpiGeneric
        : public FlashQuadSpi
    {
    public:
        static const uint8_t statusFlagWriteInProgress = 1;

        FlashQuadSpiGeneric(hal::QuadSpi& spi, FlashGeometryQuad& geometry);

        void ReadBuffer(infra::ByteRange buffer, uint32_t address, infra::Function<void()> onDone) override;

    private:
        void PageProgram() override;
        void WriteEnable() override;
        void EraseSomeSectors(uint32_t endIndex) override;
        void SendEraseSubSector(uint32_t sectorIndex);
        void SendEraseSector(uint32_t sectorIndex);
        void SendEraseBulk();
        void HoldWhileWriteInProgress() override;

    private:
        FlashGeometryQuad& geometry;
    };
}

#endif
